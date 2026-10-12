"""Generate the compact Kingdom Key mesh used by Multiplayer Safe rendering.

Standard-library only. The source JSON is never modified.
Usage: python tools/generate_keyblade_safe_lod.py
"""
from __future__ import annotations
import copy
import json
import math
from pathlib import Path
import tempfile
from export_mesh import export

HERE = Path(__file__).resolve().parent
SOURCE = HERE / 'kingdom-key-mesh.json'
OUTPUT = HERE.parent / 'res' / 'kingdom_key_safe.mesh'
TARGETS = {
    'Gold_Guard': 700,
    'Blade_and_Blue_Collar': 700,
    'Gold_Upper_Collar': 250,
    'Gold_Pommel': 250,
    'Gold_Chain_Anchor': 60,
    'Black_Grip': 96,
    'Mickey_Charm': 180,
    **{f'Chain_{i:02d}': 36 for i in range(1, 15)},
}

def simplify(mesh: dict, target: int, use_normals: bool) -> dict:
    vertices = mesh['vertices']
    triangles = mesh['triangles']
    if len(triangles) <= target:
        return copy.deepcopy(mesh)
    mins = [min(v[i] for v in vertices) for i in range(3)]
    maxs = [max(v[i] for v in vertices) for i in range(3)]
    diagonal = math.sqrt(sum((maxs[i] - mins[i]) ** 2 for i in range(3))) or 1.0

    def key_for(vertex, cell):
        material = int(vertex[8])
        position = tuple(int(math.floor((vertex[i] - mins[i]) / cell + 1e-9)) for i in range(3))
        if not use_normals:
            return (material, *position)
        normal = tuple(int(round(component * 2)) for component in vertex[3:6])
        return (material, *normal, *position)

    def cluster(cell):
        sums, counts, keys = {}, {}, []
        for vertex in vertices:
            key = key_for(vertex, cell)
            if key not in sums:
                sums[key] = [0.0] * 6
                counts[key] = 0
                keys.append(key)
            total = sums[key]
            for i in range(3):
                total[i] += vertex[i]
                total[3 + i] += vertex[3 + i]
            counts[key] += 1
        key_index = {key: i for i, key in enumerate(keys)}
        compact_vertices = []
        for key in keys:
            total, count = sums[key], counts[key]
            position = [total[i] / count for i in range(3)]
            normal = [total[3 + i] / count for i in range(3)]
            length = math.sqrt(sum(component * component for component in normal)) or 1.0
            normal = [component / length for component in normal]
            compact_vertices.append(position + normal + [0.0, 0.0, key[0]])
        remap = [key_index[key_for(vertex, cell)] for vertex in vertices]
        compact_triangles, seen = [], set()
        for triangle in triangles:
            a, b, c = (remap[index] for index in triangle)
            if len({a, b, c}) < 3:
                continue
            canonical = tuple(sorted((a, b, c)))
            if canonical in seen:
                continue
            seen.add(canonical)
            compact_triangles.append([a, b, c])
        result = copy.deepcopy(mesh)
        result['vertices'] = compact_vertices
        result['triangles'] = compact_triangles
        return result

    low, high, best = diagonal / 10000.0, diagonal * 2.0, None
    for _ in range(40):
        middle = (low + high) / 2.0
        candidate = cluster(middle)
        if len(candidate['triangles']) > target:
            low = middle
        else:
            best, high = candidate, middle
    return best or cluster(high)

def main():
    model = json.loads(SOURCE.read_text(encoding='utf-8'))
    compact = copy.deepcopy(model)
    compact['meshes'] = []
    for mesh in model['meshes']:
        target = TARGETS.get(mesh['name'], max(50, int(len(mesh['triangles']) * 0.1)))
        compact['meshes'].append(simplify(mesh, target, not mesh['bone'].startswith('Chain_')))
    with tempfile.TemporaryDirectory() as temp:
        temp_json = Path(temp) / 'kingdom-key-safe.json'
        temp_json.write_text(json.dumps(compact, separators=(',', ':')), encoding='utf-8')
        result = export(temp_json, OUTPUT, version=2)
    source_triangles = sum(len(mesh['triangles']) for mesh in model['meshes'])
    safe_triangles = result['validation']['triangles']
    print(f'Kingdom Key safe LOD: {source_triangles:,} -> {safe_triangles:,} triangles '
          f'({safe_triangles/source_triangles:.1%}); wrote {OUTPUT}')

if __name__ == '__main__':
    main()
