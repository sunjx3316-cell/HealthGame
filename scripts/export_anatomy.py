"""Export selected Z-Anatomy surfaces without executing upstream scripts.
Run Blender --factory-startup --disable-autoexec -b Startup.blend --python this.py -- output-directory
Derived mesh assets: CC BY-SA 4.0, attribution in assets/anatomy/NOTICE.md.
"""
import bpy, bmesh, numpy as np, json, sys, re, hashlib, zlib, struct
from pathlib import Path
from mathutils import Vector

OUT=Path(sys.argv[sys.argv.index('--')+1]); OUT.mkdir(parents=True, exist_ok=True)
ROT=np.array([[1,0,0],[0,0,1],[0,-1,0]],dtype=np.float32)
ORIGIN=np.array([0,.85,0],dtype=np.float32)
groups={n:[] for n in ['heart','lungs','liver','stomach','skin','body']}
labels={'heart':'心脏','lungs':'肺部','liver':'肝脏','stomach':'胃部','skin':'皮肤','body':''}
core={'heart':[], 'lungs':[], 'liver':[], 'stomach':[], 'skin':[]}
def col(o,n):return any(c.name==n for c in o.users_collection)
def names(o):return [m.name for m in o.data.materials if m]
def eligible(o):
    return o.type in {'MESH','CURVE'} and not o.name.endswith(('.j','.g','.el','.er')) and not o.name.endswith('-profile')
def classify(o):
    n=o.name.lower(); mats=names(o)
    if any(m.startswith('Skin-') for m in mats) and col(o,'9: Regions of human body'):return ('body',2)
    if col(o,'Heart'):
        return ('heart',1 if any(x in n for x in ['papillary','leaflet','chord','trabecul']) else 0)
    if col(o,'Cardiac veins') or re.search(r'coronary artery|interventricular branch|circumflex branch|marginal branch.*coronary|nodal branch',n) or n in ['ascending aorta','arch of aorta','aortic arch','pulmonary trunk','bifurcation of pulmonary trunk','superior vena cava']:
        return ('heart',0)
    if col(o,'Lungs'): return ('lungs',0)
    if col(o,'Bronchi') or col(o,'Pulmonary arteries') or col(o,'Pulmonary veins'):return ('lungs',0 if n=='trachea' else 1)
    if n in ['oropharynx','palatine tonsil.l','palatine tonsil.r']:return ('lungs',5)
    if n in ['liver','gallbladder','hepatic veins','hepatic portal vein','proper hepatic artery','common hepatic artery','bile duct'] or col(o,'Liver'):
        return ('liver',0 if n in ['liver','gallbladder'] else 1)
    if n in ['stomach','mucosa of stomach','pancreas','oesophagus','duodenum','left gastric artery','left gastro-omental artery','right gastro-omental artery','vermiform appendix','cecum','caecum','jejunum','ileum','ascending colon','transverse colon','descending colon','sigmoid colon','rectum','tongue']:
        return ('stomach', 0 if n=='stomach' else 1 if n=='mucosa of stomach' else 5)
    if col(o,'1: Skeletal system') and 'inner ear' not in n and not any(x in n for x in ['malleus','incus','stapes','tooth','teeth']):
        return ('body',3)
    return None

def geometry(o,quality=True):
    dg=bpy.context.evaluated_depsgraph_get(); ev=o.evaluated_get(dg); me=ev.to_mesh()
    if not me or not me.polygons:
        ev.to_mesh_clear();return None
    me.calc_loop_triangles()
    v=np.empty(len(me.vertices)*3,dtype=np.float32);me.vertices.foreach_get('co',v);v=v.reshape(-1,3)
    norm=np.empty_like(v);me.vertices.foreach_get('normal',norm.ravel())
    mw=np.array(o.matrix_world,dtype=np.float32)
    v=(v@mw[:3,:3].T+mw[:3,3])@ROT.T
    normalmat=np.linalg.inv(mw[:3,:3]).T
    norm=(norm@normalmat.T)@ROT.T;norm/=np.maximum(np.linalg.norm(norm,axis=1)[:,None],1e-8)
    tri=np.empty(len(me.loop_triangles)*3,dtype=np.int32);me.loop_triangles.foreach_get('vertices',tri);tri=tri.reshape(-1,3)
    if np.linalg.det(mw[:3,:3])<0:tri=tri[:,[0,2,1]]
    # Stable cavity from mesh curvature: emphasizes actual folds, not fake painted grooves.
    edges=np.empty(len(me.edges)*2,dtype=np.int32);me.edges.foreach_get('vertices',edges);edges=edges.reshape(-1,2)
    sums=np.zeros_like(v);counts=np.zeros(len(v),dtype=np.float32);lengths=np.zeros(len(v),dtype=np.float32)
    for a,b in [(edges[:,0],edges[:,1]),(edges[:,1],edges[:,0])]:
        np.add.at(sums,a,v[b]);np.add.at(counts,a,1);np.add.at(lengths,a,np.linalg.norm(v[b]-v[a],axis=1))
    diff=sums/np.maximum(counts[:,None],1)-v
    cavity=np.clip(1-np.maximum(np.sum(diff*norm,axis=1),0)/np.maximum(lengths/np.maximum(counts,1),1e-6)*2.8,.48,1)
    packed=np.concatenate([v,norm,cavity[:,None]],axis=1)[tri.ravel()]
    if o.name=='Stomach':
        # The atlas includes a circular teaching window in the stomach surface.
        # Restore that display opening for the intact view, keeping the two
        # physiological orifices. The in-game cut plane provides the section.
        all_edges=np.concatenate([tri[:,[0,1]],tri[:,[1,2]],tri[:,[2,0]]])
        unique,counts_e=np.unique(np.sort(all_edges,axis=1),axis=0,return_counts=True)
        boundary=unique[counts_e==1];neighbors={}
        for a,b in boundary:neighbors.setdefault(int(a),[]).append(int(b));neighbors.setdefault(int(b),[]).append(int(a))
        unseen=set(neighbors);loops=[]
        while unseen:
            start=next(iter(unseen));loop=[start];prev=-1;cur=start
            for _ in range(len(neighbors)+1):
                unseen.discard(cur);options=[x for x in neighbors[cur] if x!=prev]
                if not options:break
                nxt=options[0]
                if nxt==start:break
                if nxt in loop:break
                loop.append(nxt);prev,cur=cur,nxt
            if len(loop)>4:loops.append(loop)
        if loops:
            loop=max(loops,key=lambda ids:np.linalg.norm(v[ids]-np.roll(v[ids],1,axis=0),axis=1).sum())
            ring=v[loop];cn=ring.mean(0);nn=norm[loop].mean(0);nn/=max(np.linalg.norm(nn),1e-8)
            radius=np.linalg.norm(ring-cn,axis=1).mean();grid=[]
            for j in range(13):
                t=j/12;pos=cn+t*(ring-cn)+nn*radius*.48*(1-t*t)
                ns=norm[loop]*t+nn*(1-t);ns/=np.maximum(np.linalg.norm(ns,axis=1)[:,None],1e-8)
                grid.append(np.concatenate([pos,ns,np.ones((len(loop),1))],axis=1))
            cap=[]
            for j in range(12):
                for k in range(len(loop)):
                    kk=(k+1)%len(loop)
                    for face in [[grid[j][k],grid[j+1][k],grid[j+1][kk]],[grid[j][k],grid[j+1][kk],grid[j][kk]]]:
                        a=np.array(face)
                        if np.dot(np.cross(a[1,:3]-a[0,:3],a[2,:3]-a[0,:3]),nn)<0:a=a[[0,2,1]]
                        cap.extend(a)
            packed=np.concatenate([packed,np.array(cap,dtype=np.float32)])
            print('Restored stomach display window',len(loop),'boundary vertices',flush=True)
    ev.to_mesh_clear()
    return packed

def material(group,name,layer):
    n=name.lower()
    if group=='body':
        return ([.75,.67,.54,1],.66) if layer==3 else ([.73,.52,.40,1],.53)
    if 'vein' in n or 'sinus' in n or ('pulmonary' in n and 'arter' in n): return ([.22,.35,.52,1],.35)
    if 'arter' in n or 'aorta' in n or 'aortic' in n or 'coronary' in n and 'leaflet' not in n:return ([.66,.10,.12,1],.32)
    if any(x in n for x in ['leaflet','chord']):return ([.86,.73,.59,1],.42)
    if 'bronch' in n or 'trachea' in n:return ([.76,.64,.49,1],.53)
    if any(x in n for x in ['gallbladder','bile duct']):return ([.25,.37,.16,1],.3)
    if group=='heart':return ([.53,.18,.15,1],.43)
    if group=='lungs':
        # restrained lobe differences, not the atlas's rainbow segmentation palette
        shade=.025 if 'inferior' in n else -.015 if 'middle' in n else 0
        return ([.69+shade,.43+shade,.42+shade,1],.61)
    if group=='liver':return ([.43,.19,.14,1],.32)
    if 'mucosa' in n:return ([.68,.28,.25,1],.46)
    if 'pancreas' in n:return ([.72,.52,.32,1],.52)
    return ([.72,.43,.34,1],.42)

objects=[o for o in bpy.data.objects if eligible(o) and classify(o)]
# Evaluate only the selected anatomical structures. Upstream scene visibility
# must not silently disable modifiers on hidden organs.
scene=bpy.data.scenes.new('HealthGame export');bpy.context.window.scene=scene
for o in objects:
    scene.collection.objects.link(o);o.hide_viewport=False;o.hide_set(False)
    group,layer=classify(o)
    skin=any(x.startswith('Skin-') for x in names(o))
    for m in o.modifiers:
        if m.type=='SUBSURF':m.levels=2 if group!='body' else min(m.levels,1);m.show_viewport=True
        if m.type=='SOLIDIFY' and skin:m.show_viewport=False
    if skin and 'forearm.l' in o.name:
        m=o.modifiers.new('Smooth anatomical surface','SUBSURF');m.levels=3
    if o.type=='CURVE':
        o.data.resolution_u=min(16,max(8,o.data.resolution_u));o.data.bevel_resolution=min(3,max(2,o.data.bevel_resolution))
# Curve profiles are dependency objects, not exportable anatomy.
for o in list(bpy.data.objects):
    if o.name.endswith('-profile') and o.name not in scene.objects:scene.collection.objects.link(o)
bpy.context.view_layer.update()
print('Selected anatomical structures',len(objects),flush=True)
for i,o in enumerate(objects):
    group,layer=classify(o)
    packed=geometry(o,group!='body')
    if packed is None:continue
    color,rough=material(group,o.name,layer)
    part=dict(name=o.name,layer=layer,vertices=packed,color=color,roughness=rough,organ=labels[group])
    groups[group].append(part)
    if group in core and layer==0 and (group!='heart' or o.name in ['Left ventricle','Right ventricle','Left atrium','Right atrium']):
        core[group].append(packed[:,:3])
    if i%40==0:print(i,o.name,len(packed)//3,flush=True)

# A real skin region remains linked to its full-body position. Tissue interior is
# represented by the author's inner surface, not a floating cube.
forearm=[p for p in groups['body'] if 'forearm.l' in p['name']]
for p in forearm:
    q={**p,'vertices':p['vertices'].copy(),'organ':'皮肤','layer':0}
    groups['skin'].append(q);core['skin'].append(q['vertices'][:,:3])
    q2={**q,'vertices':q['vertices'].copy(),'layer':1,'name':q['name']+' / dermal surface'}
    q2['vertices'][:,:3]-=q2['vertices'][:,3:6]*.0008
    q2['color']=[.65,.31,.24,1]
    groups['skin'].append(q2)
if not core['skin']:raise RuntimeError('No forearm skin found')

# Export origins preserve atlas registration. Only these matrices place organs in body.
manifest=dict(format='HealthGame-Anatomy-1',stride=7,source='Z-Anatomy / BodyParts3D',license='CC-BY-SA-4.0',models={},placements={})
for key,parts in groups.items():
    if key=='body':continue
    xyz=np.concatenate(core[key]); lo=xyz.min(0);hi=xyz.max(0);center=(lo+hi)*.5
    scale=2.5/max(hi-lo)
    manifest['placements'][labels[key]]=dict(center=((center-ORIGIN)*2).tolist(),scale=float(2/scale))
    for p in parts:
        world=p['vertices'].copy()
        if key!='skin':
            # Overview includes the same meshes at lower detail; everything is atlas aligned.
            q={**p,'vertices':world.copy()}
            q['vertices'][:,:3]=(q['vertices'][:,:3]-ORIGIN)*2
            if q['layer']==5:q['layer']=0
            groups['body'].append(q)
        p['vertices'][:,:3]=(p['vertices'][:,:3]-center)*scale
    export=[]
    with (OUT/(key+'.bin')).open('wb') as f:
        for p in parts:
            a=p['vertices'].astype('<f4')
            export.append({k:v for k,v in p.items() if k!='vertices'}|dict(offset=f.tell(),count=len(a),center=a[:,:3].mean(0).tolist()))
            f.write(a.tobytes())
    manifest['models'][labels[key]]=dict(file=key+'.bin',parts=export,center=[0,0,0],radius=1.53,triangles=sum(p['count']//3 for p in export))
# Existing skin/bone parts are still in meter coordinates; organ parts are transformed above.
for p in groups['body']:
    if not p['organ']:p['vertices'][:,:3]=(p['vertices'][:,:3]-ORIGIN)*2
    if p['layer']==2:p['organ']='皮肤'
export=[]
with (OUT/'body.bin').open('wb') as f:
    for p in groups['body']:
        a=p['vertices'].astype('<f4')
        export.append({k:v for k,v in p.items() if k!='vertices'}|dict(offset=f.tell(),count=len(a),center=a[:,:3].mean(0).tolist()))
        f.write(a.tobytes())
manifest['models']['']=dict(file='body.bin',parts=export,center=[0,0,0],radius=1.85,triangles=sum(p['count']//3 for p in export))
for m in manifest['models'].values():
    source=OUT/m['file'];raw=source.read_bytes();target=source.with_suffix('.mesh')
    target.write_bytes(struct.pack('>I',len(raw))+zlib.compress(raw,9));m['file']=target.name
    if len(zlib.decompress(target.read_bytes()[4:]))!=len(raw):raise RuntimeError('Compression verification failed')
    source.unlink() # only this exporter's verified intermediate, never source atlas
(OUT/'atlas.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
for key,m in manifest['models'].items():print(key,'triangles',m['triangles'],'parts',len(m['parts']),flush=True)
print('EXPORT COMPLETE',flush=True)
