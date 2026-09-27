#!/usr/bin/env python3
"""Sequential N100 FFmpeg/VMAF A/B benchmark; input videos remain read-only."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import time

def compare(a, b, frames):
    if len(a['frames']) != frames or len(b['frames']) != frames:
        raise ValueError(f'Expected {frames} scored frames in each output')
    if [f['frameNum'] for f in a['frames']] != list(range(frames)):
        raise ValueError('Expected contiguous frame indices starting at zero')
    errors = {}
    vmaf_deltas = []
    for x, y in zip(a['frames'], b['frames']):
        if x['frameNum'] != y['frameNum'] or x['metrics'].keys() != y['metrics'].keys():
            raise ValueError('Frame indices or metric names differ')
        for k, v in x['metrics'].items():
            w = y['metrics'][k]
            if not math.isfinite(v) or not math.isfinite(w):
                raise ValueError('Non-finite frame score')
            errors[k] = max(errors.get(k, 0), abs(v-w))
            if k == 'vmaf':
                vmaf_deltas.append(abs(v-w))
    if a['pooled_metrics'].keys() != b['pooled_metrics'].keys():
        raise ValueError('Pooled metric names differ')
    pooled = 0
    for k, stats in a['pooled_metrics'].items():
        if stats.keys() != b['pooled_metrics'][k].keys():
            raise ValueError('Pooled statistic names differ')
        for name, value in stats.items():
            other = b['pooled_metrics'][k][name]
            if not math.isfinite(value) or not math.isfinite(other):
                raise ValueError('Non-finite pooled score')
            pooled = max(pooled, abs(value-other))
    return {'max_abs_by_metric': errors, 'max_abs_metric': max(errors.values()),
            'max_abs_vmaf': errors['vmaf'], 'max_abs_pooled': pooled,
            'mean_abs_vmaf_diff': statistics.mean(vmaf_deltas),
            'mean_vmaf_diff': abs(a['pooled_metrics']['vmaf']['mean'] -
                                  b['pooled_metrics']['vmaf']['mean'])}


def thermal():
    values = {}
    for p in Path('/sys/class/thermal').glob('thermal_zone*/temp'):
        try:
            label = p.with_name('type').read_text().strip()
            values[f'{p.parent.name}:{label}'] = int(p.read_text()) / 1000
        except (OSError, ValueError):
            pass
    return values


def command(spec, args, q, output):
    cmd = [spec.get('binary', args.binary), '-hide_banner', '-nostdin', '-v', spec.get('loglevel', 'warning'),
           '-filter_complex_threads', str(spec.get('filter_threads', 1))]
    if any(spec.get(label, 'vaapi') == 'qsv' for label in ('ref', 'dist')):
        cmd += ['-init_hw_device', 'qsv=qsv0:hw,child_device=/dev/dri/renderD128,child_device_type=vaapi']
    formats = []
    for label, source in [('dist', str(args.videos/f'{args.prefix}-q{q}.mp4')), ('ref', args.reference)]:
        decoder = spec.get(label, 'vaapi')
        if decoder == 'vaapi':
            cmd += ['-hwaccel', 'vaapi', '-hwaccel_device', '/dev/dri/renderD128',
                    '-hwaccel_output_format', 'vaapi']
        elif decoder == 'qsv':
            cmd += ['-hwaccel', 'qsv', '-hwaccel_device', 'qsv0',
                    '-hwaccel_output_format', 'qsv', '-c:v',
                    'hevc_qsv' if label == 'dist' else 'h264_qsv',
                    '-async_depth', str(spec.get('async_depth', 4)), '-gpu_copy', 'default']
        elif decoder != 'software':
            raise ValueError('Unsupported decoder: ' + decoder)
        cmd += ['-ss', str(args.seek), '-threads', str(spec.get(label+'_threads', spec.get('decode_threads', 2))),
                '-i', source]
        pre = ''
        if decoder in ('vaapi', 'qsv'):
            transfer = spec.get('transfer', 'download')
            if decoder == 'qsv' and transfer != 'download':
                raise ValueError('This diagnostic supports only QSV hwdownload')
            if transfer not in ('download', 'map_read', 'map_direct'):
                raise ValueError('Unknown VAAPI transfer mode')
            pre = {'download':'hwdownload', 'map_read':'hwmap=mode=read',
                   'map_direct':'hwmap=mode=read+direct'}[transfer]
            if spec.get('detach_sw'):
                if transfer == 'download':
                    raise ValueError('detach_sw requires a mapped software frame')
                pre += ':detach_sw=1'
            pre += ',format=nv12,'
        pre += f'trim=end_frame={args.frames},settb=AVTB,setpts=PTS-STARTPTS,'
        layout = spec.get('layout', 'lean')
        if layout in ('legacy', 'direct'):
            pre += 'scale=1920:1080:force_original_aspect_ratio=decrease:flags=bicubic,'
            pre += 'pad=1920:1080:(ow-iw)/2:(oh-ih)/2,'
        elif layout != 'lean':
            raise ValueError('Unknown preprocessing layout')
        pre += 'format=' + ('p010le' if layout == 'legacy' else spec.get('pixel_format', 'yuv420p10le'))
        formats.append(pre)
    model_cfg = '\\:'.join([f'path={args.model}', 'cambi.enc_width=1920',
                            'cambi.enc_height=1080', 'cambi.enc_bitdepth=8'])
    scoring = f"libvmaf=model='{model_cfg}':log_fmt=json:log_path={output}:n_threads={spec.get('vmaf_threads', 4)}:n_subsample=1:shortest=1:repeatlast=0"
    if 'pool' in spec:
        scoring += ':picture_pool=' + str(spec['pool'])
    if 'pool_size' in spec:
        scoring += ':picture_pool_size=' + str(spec['pool_size'])
    if 'wrap' in spec:
        scoring += ':picture_wrap=' + str(spec['wrap'])
    graph = f'[0:v:0]{formats[0]}[d];[1:v:0]{formats[1]}[r];[d][r]{scoring}[out]'
    return cmd + ['-filter_complex', graph, '-map', '[out]', '-an', '-sn', '-dn', '-f', 'null', '-']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--variants', type=Path, required=True, help='JSON list; first variant is accuracy baseline')
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--binary', required=True, help='Installed FFmpeg baseline')
    p.add_argument('--model', required=True, help='Explicit VMAF v1 model JSON')
    p.add_argument('--reference', required=True)
    p.add_argument('--videos', type=Path, required=True)
    p.add_argument('--prefix', default='pycon')
    p.add_argument('--qualities', type=int, nargs='+', default=[22], choices=[16,18,20,22,24])
    p.add_argument('--frames', type=int, default=300)
    p.add_argument('--seek', type=float, default=60.06)
    p.add_argument('--repeats', type=int, default=3)
    p.add_argument('--near', action='store_true', help='Allow VMAF frame delta <= .01 and mean absolute delta <= .001')
    args = p.parse_args()
    if min(args.frames, args.repeats) < 1 or args.seek < 0:
        p.error('Positive frames/repeats and nonnegative seek required')
    variants = json.loads(args.variants.read_text())
    names = [v['name'] for v in variants]
    if not names or len(set(names)) != len(names) or any(not n.replace('_','').replace('-','').isalnum() for n in names):
        p.error('Variant names must be unique and filename-safe')
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    metadata = {'variants':variants, 'frames':args.frames, 'seek':args.seek,
                'repeats':args.repeats, 'qualities':args.qualities,
                'reference':args.reference, 'videos':str(args.videos), 'prefix':args.prefix, 'model':args.model,
                'model_sha256':hashlib.sha256(Path(args.model).read_bytes()).hexdigest(),
                'binary_sha256':{b:hashlib.sha256(Path(b).read_bytes()).hexdigest()
                                 for b in sorted({v.get('binary',args.binary) for v in variants})},
                'baseline_variant':names[0], 'near_allowed':args.near,
                'timing':'Sequential process wall time through flush/JSON/exit, initialization and decoding included'}
    (out/'environment.json').write_text(json.dumps(metadata,ensure_ascii=False,indent=2))
    env = os.environ.copy()
    env['VMAF_INTEL_OPENCL'] = 'off'
    baselines = {}
    runs = []
    for repeat in range(args.repeats):
        order = variants[repeat % len(variants):] + variants[:repeat % len(variants)]
        for q in args.qualities:
            for spec in order:
                stem = f'q{q}-{spec["name"]}-{repeat}'
                score = out/(stem+'.json')
                cmd = command(spec, args, q, score)
                (out/(stem+'-command.json')).write_text(json.dumps(cmd,ensure_ascii=False,indent=2))
                stat_path = out/(stem+'-resources.json')
                wrapped = ['/usr/bin/time', '-f', '{"elapsed":%e,"user":%U,"system":%S,"max_rss_kb":%M,"voluntary_cs":%w,"involuntary_cs":%c}', '-o', str(stat_path)] + cmd
                before = thermal()
                started = time.perf_counter()
                run_env = env.copy()
                run_env['VMAF_SPEED_FUSED_DEC16'] = str(spec.get('fused_dec16', 0))
                run_env['VMAF_INTEL_OPENCL'] = spec.get('opencl_mode', 'off')
                if 'opencl_features' in spec:
                    run_env['VMAF_INTEL_OPENCL_FEATURES'] = spec['opencl_features']
                else:
                    run_env.pop('VMAF_INTEL_OPENCL_FEATURES', None)
                with (out/(stem+'.log')).open('w') as log:
                    subprocess.run(wrapped, env=run_env, stdout=log, stderr=log, check=True,
                                   timeout=max(180, args.frames/3))
                seconds = time.perf_counter()-started
                result = json.loads(score.read_text())
                opencl_jobs = {}
                if spec.get('opencl_mode') == 'required':
                    for feature, jobs in re.findall(r'\[vmaf-intel-opencl\] feature=(motion|adm) jobs=(\d+)',
                                                    (out/(stem+'.log')).read_text()):
                        opencl_jobs[feature] = opencl_jobs.get(feature, 0) + int(jobs)
                    requested = spec.get('opencl_features', 'motion,adm').split(',')
                    if any(opencl_jobs.get(feature, 0) < 1 for feature in requested):
                        raise RuntimeError(f'Strict OpenCL was requested but no GPU jobs recorded: {opencl_jobs}')
                pictures = None
                if spec.get('wrap'):
                    counters = re.findall(r'VMAF pictures: wrapped=(\d+) copied=(\d+) fallback=(\d+)',
                                          (out/(stem+'.log')).read_text())
                    # FFmpeg can destroy a probe filter before processing frames.
                    counters = [c for c in counters if any(map(int, c))]
                    if len(counters) != 1:
                        raise RuntimeError('Expected one imported-picture counter in FFmpeg log')
                    pictures = dict(zip(('wrapped', 'copied', 'fallback'), map(int, counters[0])))
                    if pictures != {'wrapped':2*args.frames, 'copied':0, 'fallback':0}:
                        raise RuntimeError(f'Frame import path was not used exclusively: {pictures}')
                if q not in baselines:
                    if spec['name'] != names[0]:
                        raise RuntimeError('Baseline must run first for each quality')
                    baselines[q] = result
                diff = compare(baselines[q], result, args.frames)
                run = {'quality':q, 'variant':spec['name'], 'repeat':repeat,
                       'seconds':seconds, 'fps':args.frames/seconds,
                       'resources':json.loads(stat_path.read_text()),
                       'temperature_before':before, 'temperature_after':thermal(),
                       'mean_vmaf':result['pooled_metrics']['vmaf']['mean'], 'revision':result['version'],
                       'comparison':diff, 'picture_counts':pictures, 'opencl_jobs':opencl_jobs}
                runs.append(run)
                (out/'runs.json').write_text(json.dumps(runs,indent=2,ensure_ascii=False))
                print(json.dumps({k:run[k] for k in ('quality','variant','repeat','seconds','fps','mean_vmaf')} |
                                 {'max_metric_delta':diff['max_abs_metric'],'max_vmaf_delta':diff['max_abs_vmaf']},ensure_ascii=False),flush=True)
                acceptable = (diff['max_abs_vmaf'] <= .01 and diff['mean_abs_vmaf_diff'] <= .001) if args.near else diff['max_abs_metric'] == 0 and diff['max_abs_pooled'] == 0
                if not acceptable:
                    raise RuntimeError('Score parity failed; inspect saved outputs before continuing')
    summary = {'environment':metadata, 'results':[]}
    for q in args.qualities:
        base = statistics.median(r['seconds'] for r in runs if r['quality']==q and r['variant']==names[0])
        for name in names:
            group = [r for r in runs if r['quality']==q and r['variant']==name]
            median = statistics.median(r['seconds'] for r in group)
            summary['results'].append({'quality':q,'variant':name,'median_seconds':median,
                'min_seconds':min(r['seconds'] for r in group),'max_seconds':max(r['seconds'] for r in group),
                'fps':args.frames/median,'time_reduction_percent':100*(1-median/base),
                'speedup':base/median,'max_abs_metric':max(r['comparison']['max_abs_metric'] for r in group),
                'max_abs_vmaf':max(r['comparison']['max_abs_vmaf'] for r in group),
                'max_mean_vmaf_delta':max(r['comparison']['mean_vmaf_diff'] for r in group),
                'max_mean_abs_vmaf_delta':max(r['comparison']['mean_abs_vmaf_diff'] for r in group),
                'median_cpu_seconds':statistics.median(r['resources']['user']+r['resources']['system'] for r in group),
                'peak_rss_kb':max(r['resources']['max_rss_kb'] for r in group)})
    (out/'summary.json').write_text(json.dumps(summary,indent=2,ensure_ascii=False))
    print(json.dumps(summary['results'],ensure_ascii=False,indent=2),flush=True)


if __name__ == '__main__':
    main()
