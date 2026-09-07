"""Inspect the downloaded atlas without running any of its embedded scripts."""
import bpy, json, sys
from pathlib import Path
result = []
for obj in bpy.data.objects:
    if obj.type != "MESH":
        continue
    result.append({"name": obj.name, "vertices": len(obj.data.vertices),
        "polygons": len(obj.data.polygons),
        "collections": [c.name for c in obj.users_collection],
        "materials": [m.name for m in obj.data.materials if m],
        "bounds": [list(obj.matrix_world @ __import__("mathutils").Vector(c)) for c in obj.bound_box]})
out = Path(sys.argv[sys.argv.index("--")+1])
out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
print("ATLAS_MESH_COUNT", len(result))
for item in result:
    if any(term in item["name"].lower() for term in ("heart","ventric","atrium","lobe of","liver","stomach","skin","body","coronary","aort","bronch","trache","gallbladder","pancreas","esoph")):
        print(item["name"], item["vertices"], item["polygons"], item["materials"])

