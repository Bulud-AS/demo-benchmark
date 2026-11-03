using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;
using System.Xml.Linq;
namespace DemoBenchmark {
public static class Mat {
 public static float[] Identity(){return new float[]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
 public static float[] Transpose(float[] a){float[] b=new float[16];for(int i=0;i<4;i++)for(int j=0;j<4;j++)b[i*4+j]=a[j*4+i];return b;}
 public static float[] Mul(float[] a,float[] b){float[] c=new float[16];for(int i=0;i<4;i++)for(int j=0;j<4;j++)for(int k=0;k<4;k++)c[i*4+j]+=a[k*4+j]*b[i*4+k];return c;}
 public static float[] Transform(float[] m,float[] v,float w){return new float[]{m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+m[12]*w,m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+m[13]*w,m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+m[14]*w};}
 static float[] Norm(float[] a){double len=Math.Sqrt(a.Sum(x=>x*x));return a.Select(x=>(float)(x/len)).ToArray();}
 static float[] Cross(float[] a,float[] b){return new float[]{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
 static float Dot(float[] a,float[] b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
 public static float[] View(float[] eye,float[] look,float[] up){float[] z=Norm(look.Select(v=>-v).ToArray()),x=Norm(Cross(up,z)),y=Cross(z,x);return new float[]{x[0],y[0],z[0],0,x[1],y[1],z[1],0,x[2],y[2],z[2],0,-Dot(x,eye),-Dot(y,eye),-Dot(z,eye),1};}
 // Source cameras use horizontal FOV and a +Z forward axis. Root units also
 // scale their clipping distances; the OpenGL projection preserves those values.
 public static float[] Projection(Camera c,float aspect,float unit){float f=1f/(float)Math.Tan(c.fov*.5);float n=c.near*unit,z=c.far*unit;return new float[]{-f,0,0,0,0,f*aspect,0,0,0,0,(z+n)/(n-z),-1,0,0,2*z*n/(n-z),0};}
 public static float[] TRS(float[] p,float[] q,float[] s){float w=q[0],x=q[1],y=q[2],z=q[3];return new float[]{(1-2*y*y-2*z*z)*s[0],(2*x*y+2*w*z)*s[0],(2*x*z-2*w*y)*s[0],0,(2*x*y-2*w*z)*s[1],(1-2*x*x-2*z*z)*s[1],(2*y*z+2*w*x)*s[1],0,(2*x*z+2*w*y)*s[2],(2*y*z-2*w*x)*s[2],(1-2*x*x-2*y*y)*s[2],0,p[0],p[1],p[2],1};}
 public static float[] Key(double[][] keys,double time,bool quaternion){if(keys.Length==0)return quaternion?new float[]{1,0,0,0}:new float[]{1,1,1};int lo=0,hi=keys.Length-1;while(lo+1<hi){int mid=(lo+hi)/2;if(keys[mid][0]<=time)lo=mid;else hi=mid;}if(time>=keys[hi][0])lo=hi;double[] a=keys[lo],b=keys[Math.Min(lo+1,keys.Length-1)];double t=a==b?0:Math.Max(0,Math.Min(1,(time-a[0])/(b[0]-a[0])));float[] v=new float[a.Length-1];double sign=1,wa=1-t,wb=t;if(quaternion){double dot=0;for(int i=1;i<a.Length;i++)dot+=a[i]*b[i];if(dot<0){sign=-1;dot=-dot;}if(dot<.9995){double angle=Math.Acos(Math.Min(1,dot)),sn=Math.Sin(angle);wa=Math.Sin((1-t)*angle)/sn;wb=Math.Sin(t*angle)/sn;}}
 for(int i=0;i<v.Length;i++)v[i]=(float)(a[i+1]*wa+b[i+1]*wb*sign);if(quaternion){double len=Math.Sqrt(v.Sum(x=>x*x));for(int i=0;i<v.Length;i++)v[i]/=(float)len;}return v;}
}
public class SceneNode {public string name;public int parent;public float[] matrix;public int[] meshes;[ScriptIgnore]public float[] World;[ScriptIgnore]public Channel Channel;}
public class Camera {public string name;public float[] position,look,up;public float fov,near,far,aspect;}
public class Channel {public string name;public double[][] positions,rotations,scales;public int pre,post;}
public class Animation {public string name;public double duration,tps;public Channel[] channels;}
public class SceneMesh {public int offset,indexOffset,vertices,indices,material;[ScriptIgnore]public uint Vao,Vbo,Ebo;}
public class Scene {public SceneNode[] nodes;public SceneMesh[] meshes;public Dictionary<string,object>[] materials;public Animation[] animations;public Camera[] cameras;[ScriptIgnore]public Dictionary<string,SceneNode> Names;[ScriptIgnore]public float[][] Colors;
 public static Scene Load(string path){JavaScriptSerializer j=new JavaScriptSerializer{MaxJsonLength=int.MaxValue};Scene s=j.Deserialize<Scene>(File.ReadAllText(path));s.Names=s.nodes.ToDictionary(n=>n.name);foreach(SceneNode n in s.nodes)n.matrix=Mat.Transpose(n.matrix);if(s.animations.Length>0)foreach(Channel c in s.animations[0].channels){SceneNode n;if(s.Names.TryGetValue(c.name,out n))n.Channel=c;}
 s.Colors=new float[s.materials.Length][];for(int i=0;i<s.materials.Length;i++){object raw;s.materials[i].TryGetValue("$clr.diffuse",out raw);object[] a=raw as object[];System.Collections.ArrayList list=raw as System.Collections.ArrayList;float[] c=raw==null?new float[]{1,1,1,1}:((System.Collections.IEnumerable)raw).Cast<object>().Select(x=>Convert.ToSingle(x,CultureInfo.InvariantCulture)).ToArray();if(c.Length==3)c=new float[]{c[0],c[1],c[2],1};if(Math.Abs(c[0]-c[1])>.01||Math.Abs(c[1]-c[2])>.01)c=new float[]{1,0,0,1};s.Colors[i]=c;}return s;}
 public float[] Evaluate(double time,int camera,float aspect){double t=animations.Length>0?time*animations[0].duration:0;foreach(SceneNode n in nodes){float[] local=n.matrix;if(n.Channel!=null){Channel c=n.Channel;local=Mat.TRS(Mat.Key(c.positions,t,false),Mat.Key(c.rotations,t,true),Mat.Key(c.scales,t,false));}n.World=n.parent<0?local:Mat.Mul(nodes[n.parent].World,local);}Camera cam=cameras[Math.Max(0,Math.Min(camera,cameras.Length-1))];float[] m=Names[cam.name].World;return Mat.Mul(Mat.Projection(cam,aspect,(float)Math.Sqrt(m[0]*m[0]+m[1]*m[1]+m[2]*m[2])),Mat.View(Mat.Transform(m,cam.position,1),Mat.Transform(m,cam.look.Select(v=>-v).ToArray(),0),Mat.Transform(m,cam.up,0)));}
}
public class CurvePoint {public double Time,Value;public string Mode;}
public class Clip {public double Start,Length;public CurvePoint[] Points;public double Evaluate(double t){t-=Start;if(t<=Points[0].Time)return Points[0].Value;int i=0;while(i+1<Points.Length&&Points[i+1].Time<=t)i++;if(i+1==Points.Length)return Points[i].Value;CurvePoint a=Points[i],b=Points[i+1];double f=(t-a.Time)/(b.Time-a.Time);if(a.Mode=="hold")f=0;else if(a.Mode=="smooth")f=(1-Math.Cos(f*3.1415))*.5;else if(a.Mode=="fast")f=Math.Sqrt(f);else if(a.Mode=="slow")f=f*f;return a.Value+(b.Value-a.Value)*f;}}
public class Parameter {public double Static;public Clip[] Clips;public bool Active(double t){return Clips==null||Clips.Any(c=>t>=c.Start&&t<c.Start+c.Length);}public double Value(double t){if(Clips==null)return Static;foreach(Clip c in Clips)if(t>=c.Start&&t<c.Start+c.Length)return c.Evaluate(t);return 0;}}
public class Effect {public int Id;public string Type,Name;public Dictionary<string,Parameter> Params=new Dictionary<string,Parameter>();public bool Active(double t){return !Params.Values.Any(p=>p.Clips!=null)||Params.Values.Any(p=>p.Clips!=null&&p.Active(t));}public float Value(string name,double t,float fallback){Parameter p;return Params.TryGetValue(name,out p)?(float)p.Value(t):fallback;}}
public class GraphNode {public int Id,Resource,Effect,Target;public string Type;public List<Connection> Inputs=new List<Connection>();}
public class Connection {public int Source;public string Input;}
// Video frame matching fixes the master clock origin at 700 ms. Audio uses
// the video PCM cursor directly, so output latency does not accumulate drift.
public class Timeline {public const double Duration=5728256.0/44100.0,ClockOffset=0.700;public Dictionary<int,string> Resources=new Dictionary<int,string>();public Dictionary<int,Effect> Effects=new Dictionary<int,Effect>();public Dictionary<int,GraphNode> Nodes=new Dictionary<int,GraphNode>();
 static int I(XElement e,string k){XAttribute a=e.Attribute(k);return a==null?0:int.Parse(a.Value,CultureInfo.InvariantCulture);}static double D(XElement e,string k){return double.Parse(e.Attribute(k).Value,CultureInfo.InvariantCulture);}
 public Timeline(string path){XElement root=XDocument.Load(path).Root;foreach(XElement e in root.Elements("resource"))Resources.Add(I(e,"index"),(string)e.Attribute("file"));foreach(XElement e in root.Elements("effect")){Effect fx=new Effect{Id=I(e,"index"),Type=(string)e.Attribute("effectfuid"),Name=(string)e.Attribute("name")};foreach(XElement p in e.Elements("parameter")){Parameter par=new Parameter();XElement st=p.Element("static");if(st!=null)par.Static=D(st,"value");else par.Clips=p.Elements("clip").Select(c=>new Clip{Start=D(c,"start"),Length=D(c,"length"),Points=c.Elements("point").Select(q=>new CurvePoint{Time=D(q,"time"),Value=D(q,"value"),Mode=(string)q.Attribute("interpolation")}).OrderBy(q=>q.Time).ToArray()}).ToArray();fx.Params.Add((string)p.Attribute("name"),par);}Effects.Add(fx.Id,fx);}
 foreach(XElement n in root.Elements("node")){GraphNode g=new GraphNode{Id=I(n,"index"),Type=(string)n.Attribute("type"),Resource=I(n,"resourceid"),Effect=I(n,"effectid"),Target=I(n,"targetid")};Nodes.Add(g.Id,g);}foreach(XElement c in root.Elements("nodeconnection"))Nodes[I(c,"target")].Inputs.Add(new Connection{Source=I(c,"source"),Input=(string)c.Attribute("targetinput")});}
 public int Input(GraphNode node,string input){Connection c=node.Inputs.FirstOrDefault(x=>x.Input==input);return c==null?0:c.Source;}
 public string ResourceInput(GraphNode node,string input){int id=Input(node,input);return id==0?"":Resources[Nodes[id].Resource];}
}
}




