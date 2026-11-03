"""Build real curved-surface LODs while keeping the source node/camera animation."""
from pathlib import Path
import hashlib,json,sys
import numpy as np

source=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parent.parent.parent/'assets'
out=Path(sys.argv[2]) if len(sys.argv)>2 else source/'quality';out.mkdir(parents=True,exist_ok=True)
unique={};entries=[];stats=[]
for path in sorted(source.glob('*.json')):
    scene=json.loads(path.read_text());blob=path.with_suffix('.mesh').read_bytes()
    sphere_ids=sorted({i for n in scene['nodes'] if 'sphere' in n['name'].lower() for i in n['meshes']})
    for mid in sphere_ids:
        mesh=scene['meshes'][mid]
        raw=blob[mesh['offset']:mesh['indexOffset']+mesh['indices']*4]
        key=hashlib.sha256(raw).hexdigest()[:16]
        if key not in unique:
            vertices=np.frombuffer(blob,'<f4',mesh['vertices']*6,mesh['offset']).reshape(-1,6).astype(np.float64)
            faces=np.frombuffer(blob,'<u4',mesh['indices'],mesh['indexOffset']).reshape(-1,3).copy()
            radius=float(np.median(np.linalg.norm(vertices[:,:3],axis=1)))
            levels=[]
            for level in range(5):
                if level:
                    data=vertices.tolist();cache={};new_faces=[]
                    def midpoint(a,b):
                        edge=(min(a,b),max(a,b))
                        if edge in cache:return cache[edge]
                        va,vb=vertices[a],vertices[b];p=(va[:3]+vb[:3])*.5
                        # The two sphere geometries include flat cut faces. Only
                        # edges whose endpoints lie on the sphere follow an arc;
                        # radial/cap edges stay in their original cut plane.
                        if abs(np.linalg.norm(va[:3])-radius)<radius*1e-4 and abs(np.linalg.norm(vb[:3])-radius)<radius*1e-4:
                            distance=np.linalg.norm(p)
                            if distance>1e-9:p*=radius/distance
                        n=(va[3:]+vb[3:])*.5;length=np.linalg.norm(n)
                        if length>1e-9:n/=length
                        index=len(data);data.append(np.r_[p,n].tolist());cache[edge]=index;return index
                    for a,b,c in faces:
                        ab=midpoint(int(a),int(b));bc=midpoint(int(b),int(c));ca=midpoint(int(c),int(a))
                        new_faces.extend([(a,ab,ca),(ab,b,bc),(ca,bc,c),(ab,bc,ca)])
                    vertices=np.asarray(data,dtype=np.float64);faces=np.asarray(new_faces,dtype=np.uint32)
                assert np.isfinite(vertices).all()
                file=f'{key}-{level}.mesh';v=vertices.astype('<f4');idx=faces.astype('<u4')
                (out/file).write_bytes(v.tobytes()+idx.tobytes())
                levels.append(dict(file=file,offset=0,indexOffset=len(v)*24,vertices=len(v),indices=idx.size,material=mesh['material']))
            unique[key]=levels
            stats.append(dict(geometry=key,radius=radius,triangles=[x['indices']//3 for x in levels]))
        entries.append(dict(scene=path.stem+'.DAE',mesh=mid,levels=unique[key]))
(out/'manifest.json').write_text(json.dumps(entries,separators=(',',':')))
(out/'statistics.json').write_text(json.dumps(dict(uniqueSphereGeometries=len(unique),sphereMeshInstances=len(entries),geometries=stats),indent=2))
print(json.dumps(stats,indent=2))
