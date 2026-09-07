import bpy, json, sys
from pathlib import Path
from mathutils import Vector
data=[]
for obj in bpy.data.objects:
    if obj.type not in {'MESH','CURVE','SURFACE','META'}: continue
    if obj.name.endswith(('.j','.g')): continue
    if obj.type=='MESH' and not obj.data.polygons and not obj.modifiers: continue
    data.append(dict(name=obj.name,type=obj.type,collections=[c.name for c in obj.users_collection],
        materials=[m.name for m in obj.data.materials if m],
        modifiers=[dict(name=m.name,type=m.type,show=m.show_viewport,levels=getattr(m,'levels',None)) for m in obj.modifiers],
        bounds=[list(obj.matrix_world@Vector(c)) for c in obj.bound_box]))
Path(sys.argv[sys.argv.index('--')+1]).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
print('SURFACES',len(data))
for item in data:
    if any(x in item['name'].lower() for x in ['skin','coronary','mucosa','cutaneous','integument']) or 'Heart' in item['collections']:
        print(item)
print('COLLECTIONS', [c.name for c in bpy.data.collections])

