import json,xml.etree.ElementTree as ET
from pathlib import Path
root=Path(__file__).resolve().parent.parent
dest=root/'android/app/src/main/assets'
dest.mkdir(parents=True,exist_ok=True)
xml=ET.parse(root/'assets/timeline.xml').getroot()
data={'resources':{},'effects':{},'nodes':{}}
for r in xml.findall('resource'):data['resources'][r.get('index')]=r.get('file')
for fx in xml.findall('effect'):
    pars={}
    for p in fx.findall('parameter'):
        s=p.find('static')
        if s is not None:pars[p.get('name')]={'value':float(s.get('value'))}
        else:pars[p.get('name')]={'clips':[{'start':float(c.get('start')),'length':float(c.get('length')),'points':sorted([{'time':float(q.get('time')),'value':float(q.get('value')),'mode':q.get('interpolation','linear')} for q in c.findall('point')],key=lambda q:q['time'])} for c in p.findall('clip')]}
    data['effects'][fx.get('index')]={'type':fx.get('effectfuid'),'params':pars}
for n in xml.findall('node'):data['nodes'][n.get('index')]={'type':n.get('type'),'resource':int(n.get('resourceid','0')),'effect':int(n.get('effectid','0')),'inputs':[]}
for c in xml.findall('nodeconnection'):data['nodes'][c.get('target')]['inputs'].append({'source':int(c.get('source')),'input':c.get('targetinput')})
(dest/'timeline-android.json').write_text(json.dumps(data,separators=(',',':')),encoding='utf-8')
(dest/'CREDITS.txt').write_bytes((root/'android/CREDITS.txt').read_bytes())
(dest/'LICENSE.txt').write_bytes((root/'LICENSE.txt').read_bytes())
(dest/'JSON-LICENSE.txt').write_bytes((root/'android/third_party/nlohmann/LICENSE.MIT').read_bytes())
oboe=root/'android/third_party/oboe/LICENSE'
if oboe.exists():(dest/'Oboe-LICENSE.txt').write_bytes(oboe.read_bytes())
if oboe.exists():(dest/'AndroidX-Apache-LICENSE.txt').write_bytes(oboe.read_bytes())
stb=(root/'android/third_party/stb_image.h').read_text(encoding='utf-8')
(dest/'stb-LICENSE.txt').write_text(stb[stb.rfind('ALTERNATIVE A - MIT License'):],encoding='utf-8')
print('Android timeline and notices prepared')
