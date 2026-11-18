"""Build the four corresponding portrait blend shapes from MakeHuman HM08.

Usage: python export_portrait.py SOURCE_DIR OUTPUT_DIR (requires NumPy).
SOURCE_DIR contains base.obj and asian-male-{baby,child,young,old}.target.
The names, attribution and upstream locations are in PORTRAIT-NOTICE.md.
"""
from pathlib import Path
import json
import struct
import sys
import zlib
import numpy as np


def unit(v):
    return v / np.maximum(np.linalg.norm(v,axis=-1,keepdims=True),1e-10)


def smooth(a,b,v):
    t=np.clip((v-a)/(b-a),0,1)
    return t*t*(3-2*t)


def read_obj(path):
    verts,groups=[],{}
    for line in path.read_text().splitlines():
        fields=line.split()
        if not fields:continue
        if fields[0]=='v':verts.append(list(map(float,fields[1:4])))
        if fields[0]=='g':faces=groups.setdefault(fields[1],[])
        if fields[0]=='f':
            ids=[int(x.split('/')[0])-1 for x in fields[1:]]
            faces.extend((ids[0],ids[j],ids[j+1]) for j in range(1,len(ids)-1))
    return np.array(verts),{k:np.array(v) for k,v in groups.items()}


def register(v,g,arm,head_weight):
    def joint(name):return v[np.unique(g['joint-'+name])].mean(0)
    body=v[np.unique(g['body'])]
    eye,sh,hip,knee=map(joint,['l-eye','l-shoulder','l-upper-leg','l-knee'])
    x,y,z=v.T
    h=(.35/sh[0])
    q=np.column_stack((x*h,np.interp(y,[body[:,1].min(),knee[1],hip[1],sh[1],eye[1]],
                                     [-1.7,-.81,-.045,1.035,1.48]),(z-sh[2])*h-.02))
    # Adduct the arms into the atlas rest pose, around the actual shoulders.
    dx=np.abs(x*h)-.35;dy=(y-sh[1])*h
    bend=.41
    q[:,0]+=np.sign(x)*arm*(np.cos(bend)*dx+np.sin(bend)*dy-dx)
    q[:,1]+=arm*(-np.sin(bend)*dx+(np.cos(bend)-1)*dy)
    leg=(1-arm)*(1-smooth(knee[1],hip[1]+.2,y))
    q[:,0]*=1-.30*leg
    # Keep the *native* skull, eye spacing, nose, jaw and lips of each target.
    # Register with one similarity transform above the base of the neck.
    headscale=.161/np.percentile(abs(body[abs(body[:,1]-eye[1])<.14,0]),98)
    head=np.column_stack((x*headscale,1.48+(y-eye[1])*headscale,
                          .136+(z-eye[2])*headscale))
    w=head_weight[:,None]
    return q*(1-w)+head*w


def subdivide(frames,faces):
    # One shared Loop step gives all four shapes the same topology.
    edges=np.concatenate((faces[:,[0,1]],faces[:,[1,2]],faces[:,[2,0]]))
    edges,inv,count=np.unique(np.sort(edges,axis=1),axis=0,return_inverse=True,return_counts=True)
    opposite=faces[:,[2,0,1]].T.ravel()
    ab,bc,ca=inv.reshape(3,-1)+frames.shape[1]
    a,b,c=faces.T
    refined=np.stack(([a,ab,ca],[ab,b,bc],[ca,bc,c],[ab,bc,ca]),axis=2).transpose(1,2,0).reshape(-1,3)
    results=[]
    for v in frames:
        mids=v[edges].mean(1)
        adj=np.zeros_like(mids);np.add.at(adj,inv,v[opposite])
        mids[count==2]=.75*mids[count==2]+.125*adj[count==2]
        accum=np.zeros_like(v);val=np.zeros(len(v))
        u,w=edges.T
        np.add.at(accum,u,v[w]);np.add.at(accum,w,v[u])
        np.add.at(val,u,1);np.add.at(val,w,1)
        beta=np.where(val==3,3/16,3/(8*np.maximum(val,1)))
        moved=v+(accum-v*val[:,None])*beta[:,None]
        boundary=edges[count==1].ravel()
        moved[boundary]=v[boundary]
        results.append(np.concatenate([moved,mids]))
    return np.array(results),refined


def mesh_normals(v,f):
    normal=np.zeros_like(v);tri=v[f]
    n=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0])
    for idx in f.T:np.add.at(normal,idx,n)
    return unit(normal)


def sculpt_locks(frames,normals,faces):
    # Ribbons follow sampled points of the real scalp in all four keys. Their
    # cross section is convex and tapers towards the fringe and the temples.
    used=np.unique(faces);p=frames[0,used]
    center=np.array([0,1.545,-.025])
    keep=(p[:,1]>1.485)&(normals[0,used,1]>-.28)
    ids=used[keep];rays=unit((frames[0,ids]-center)/[.165,.217,.183])
    mesh=[];triangles=[]
    for i in range(48):
        angle=i*2*np.pi/48
        front=.5+.5*np.cos(angle)
        end=1.73-.44*front+.11*np.sin(3*angle+.6)
        path=[];up=[]
        for t in np.linspace(0,1,18):
            theta=angle+.76*(1-t)**1.35
            phi=.18+(end-.18)*t
            ray=[np.sin(phi)*np.sin(theta),np.cos(phi),np.sin(phi)*np.cos(theta)]
            score=rays@ray;near=np.argpartition(score,-4)[-4:]
            w=1/np.maximum(.00004,1-score[near]);w/=w.sum()
            path.append((frames[:,ids[near]]*w[None,:,None]).sum(1))
            up.append(unit((normals[:,ids[near]]*w[None,:,None]).sum(1)))
        start=len(mesh);lengths=np.array([.019,.005,.017,.014])[:,None]
        for j,t in enumerate(np.linspace(0,1,18)):
            across=unit(np.cross(up[j],path[min(j+1,17)]-path[max(j-1,0)]))
            taper=.05+.95*max(0,np.sin(np.pi*t))**.52
            for v in np.linspace(-1,1,5):
                mesh.append(path[j]+up[j]*lengths*(1.05+taper*(1-v*v)*.63)
                            +across*lengths*v*taper*1.12)
            if j==0:continue
            for k in range(4):
                a=start+(j-1)*5+k
                triangles.extend([[a,a+5,a+1],[a+1,a+5,a+6]])
    mesh=np.array(mesh).transpose(1,0,2);ff=np.array(triangles)
    return mesh,np.array([mesh_normals(s,ff) for s in mesh]),ff


def main(src,out):
    base,g=read_obj(src/'base.obj')
    targets=[]
    for stage in ['young','baby','child','old']:
        v=base.copy();delta=np.loadtxt(src/('asian-male-'+stage+'.target'))
        v[delta[:,0].astype(int)]+=delta[:,1:]
        targets.append(v)
    adult=targets[0]
    def joint(k):return adult[np.unique(g['joint-l-'+k])].mean(0)
    shoulder,hip,eye=map(joint,['shoulder','upper-leg','eye'])
    x,y,z=adult.T
    distal=smooth(1.34,1.59,np.abs(x)/shoulder[0])
    arm=smooth(.84,1.25,np.abs(x)/shoulder[0])*np.maximum(distal,smooth(hip[1]-.55,hip[1]+.16,y))*(1-smooth(shoulder[1],eye[1],y))
    head_weight=smooth(shoulder[1]+.07*(eye[1]-shoulder[1]),eye[1]-.54*(eye[1]-shoulder[1]),y)
    frames=np.array([register(v,g,arm,head_weight) for v in targets])
    # Align the youngest profile to a neutral gaze using the corresponding
    # chin landmark, then ease the expression at the outer corners of the lips.
    adult=frames[0];ids=np.unique(g['body'])
    chin=ids[np.argmin(np.linalg.norm(adult[ids]-[0,1.276,.181],axis=1))]
    for k in [1,2]:
        face=frames[k];dy,dz=face[chin,1:]-[1.48,.136]
        ref=frames[0,chin,1:]-[1.48,.136]
        angle=np.arctan2(ref[1],-ref[0])-np.arctan2(dz,-dy)
        a=angle*head_weight
        dy=face[:,1]-1.48;dz=face[:,2]-.136
        fy=1.48+np.cos(a)*dy+np.sin(a)*dz
        face[:,2]=.136-np.sin(a)*dy+np.cos(a)*dz;face[:,1]=fy
        x,y,z=adult.T; front=smooth(.085,.163,z)
        # Preserve the infant target's roundness while bringing out a gentle smile.
        face[:,1]+=.010*np.exp(-((abs(x)-.042)/.024)**2-((y-1.341)/.023)**2)*front
    frames[1]=.78*frames[1]+.22*frames[2]
    # A little more lid aperture in the early keys helps the eyes read at the
    # overview camera distance. Deform the lids and eyeballs in correspondence.
    x,y,z=frames[0].T
    eye_mask=np.exp(-((abs(x)-.068)/.049)**2-((y-1.48)/.034)**2)*smooth(.065,.118,z)
    for k,factor in [(1,.22),(2,.15)]:
        center=frames[k,np.unique(g['joint-l-eye'])].mean(0)
        frames[k,:,1]+=factor*eye_mask*(frames[k,:,1]-center[1])
    groups=[];chunks=[];offset=0
    def append(name,layer,color,frames,normals,faces):
        nonlocal offset
        if len(faces)==0:return
        buffers=[]
        for k in range(4):
            buffers.extend((frames[k],normals[k]))
        a=np.concatenate(buffers,axis=1)[faces.ravel()].astype('<f4')
        chunks.append(a.tobytes())
        groups.append(dict(name=name,layer=layer,color=color,first=offset,count=len(a)))
        offset+=len(a)
    for group in ['body','helper-l-eye','helper-r-eye']:
        v,f=subdivide(frames,g[group]);ns=np.array([mesh_normals(s,f) for s in v])
        centers=v[0,f].mean(1);x,y,z=centers.T;ax=abs(x)
        if group=='body':
            # Relax the underlying triangles only in clothing-covered areas.
            # This gives the jersey its own silhouette over ribs, navel and hips.
            edges=np.unique(np.sort(np.concatenate([f[:,[0,1]],f[:,[1,2]],f[:,[2,0]]]),axis=1),axis=0)
            a,b=edges.T;degree=np.bincount(edges.ravel(),minlength=v.shape[1])
            rest=v[0].copy();xx,yy,zz=rest.T
            cloth=smooth(-.41,-.37,yy)*(1-smooth(1.075,1.13,yy))
            cloth*=1-smooth(.30,.40,abs(xx))*(1-smooth(.77,.81,yy))
            for k in range(4):
                sm=v[k].copy()
                for _ in range(9):
                    neighbors=np.zeros_like(sm);np.add.at(neighbors,a,sm[b]);np.add.at(neighbors,b,sm[a])
                    sm+=.50*(neighbors/np.maximum(1,degree[:,None])-sm)*cloth[:,None]
                v[k]=sm+ns[k]*cloth[:,None]*.015
                ns[k]=mesh_normals(v[k],f)
            hand=(ax>.43)&(y<.16)&(y>-.52)
            arm=(ax>.31)&(y<.845)&(y>=.16)
            foot=(y<-1.44)
            for name,mask in [('palm and digits',hand),('forearm',arm),('sole',foot),
                              ('Patient surface',~(hand|arm|foot))]:
                append(name,2,[.85,.655,.55],v,ns,f[mask])
            # A fitted cap uses the same scalp vertices as the head. Length
            # and crown volume are sculpted separately for each age target.
            hair=v.copy()
            for k,depth in enumerate([.019,.004,.015,.012]):
                p=v[k];sweep=p[:,0]*29+p[:,2]*18+np.sin(p[:,1]*13)
                ridge=(.5+.5*np.sin(sweep*2))**2
                hair[k]+=ns[k]*((depth*(.6+.7*ridge))[:,None])
            append('Crown',9,[.19,.125,.105],hair,np.array([mesh_normals(s,f) for s in hair]),f[y>1.32])
            locks,ln,lf=sculpt_locks(v,ns,f[y>1.32])
            append('Swept locks',10,[.226,.156,.129],locks,ln,lf)
        else:
            append(group,8,[.90,.85,.79],v,ns,f)
    raw=b''.join(chunks);out.mkdir(parents=True,exist_ok=True)
    (out/'portrait.mesh').write_bytes(struct.pack('>I',len(raw))+zlib.compress(raw,7))
    (out/'portrait.json').write_text(json.dumps(dict(stride=24,parts=groups),indent=2))
    print('Portrait:',offset//3,'triangles across four registered shapes')


if __name__=='__main__':main(*map(Path,sys.argv[1:]))
