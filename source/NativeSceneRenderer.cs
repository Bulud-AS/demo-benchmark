using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace DemoBenchmark {
internal static class DirectGraphics {
    const string Library="NativeGraphics.dll";
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibraryEx(string path,IntPtr file,uint flags);
    [DllImport("kernel32.dll")] static extern uint SetErrorMode(uint mode);
    static IntPtr module;
    internal static void EnsureLoaded(){
        if(module!=IntPtr.Zero)return;int error;uint old=SetErrorMode(0x8001);
        try{module=LoadLibraryEx(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"runtime",Library),IntPtr.Zero,0x1100);error=Marshal.GetLastWin32Error();}finally{SetErrorMode(old);}
        if(module==IntPtr.Zero)throw new NotSupportedException("ネイティブ描画ライブラリを読み込めません。エラー "+error);
    }
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern IntPtr db_create(int api,IntPtr window,int width,int height,int samples);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern void db_destroy(IntPtr device);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern IntPtr db_error();
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern IntPtr db_name(IntPtr device);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern IntPtr db_vendor(IntPtr device);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern IntPtr db_version(IntPtr device);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int db_texture(IntPtr device,int w,int h,byte[] pixels,int target);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int db_mesh(IntPtr device,IntPtr vertices,int count,IntPtr indices,int indexCount);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int db_begin(IntPtr device);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int db_bind(IntPtr device,int target,int flags,float[] color);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int db_draw(IntPtr device,int mesh,float[] constants,int source,int noise);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int db_update(IntPtr device,int id,int w,int h,byte[] pixels);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int db_present(IntPtr device,int root,int nearest);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int db_capture(IntPtr device,int root,byte[] pixels);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int db_finish(IntPtr device);
    [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] internal static extern int db_vsync(IntPtr device,int enabled);
    internal static string Error {get{return Marshal.PtrToStringAnsi(db_error());}}
    internal static int Check(int status){if(status<=0)throw new NotSupportedException(Error);return status;}
}
internal sealed class NativeSceneRenderer:IDisposable {
    IntPtr device;
    readonly QualityPreset quality;readonly bool secondReality;readonly Timeline timeline;
    readonly Dictionary<string,Scene> scenes=new Dictionary<string,Scene>();
    readonly Dictionary<string,int> textures=new Dictionary<string,int>();
    readonly Dictionary<int,int> targets=new Dictionary<int,int>(),cached=new Dictionary<int,int>();
    readonly Stack<int> freeTargets=new Stack<int>();
    readonly Dictionary<int,bool> activity=new Dictionary<int,bool>();
    readonly float[] constants=new float[40],black={0,0,0,0},white={1,1,1,1};
    readonly int root,fallback;int srTexture,srSerial=-1;double time;
    public readonly string DeviceName,Version,Vendor;public long LoadedTriangles;public int DrawCalls,Triangles;
    public bool Vsync {get;private set;}
    public NativeSceneRenderer(GLSurface surface,string assets,RenderApi api,QualityPreset preset,bool second){
        quality=preset;secondReality=second;
        DirectGraphics.EnsureLoaded();
        device=DirectGraphics.db_create((int)api,surface.Handle,preset.Width,preset.Height,preset.Samples);
        if(device==IntPtr.Zero)throw new NotSupportedException(DirectGraphics.Error);
        try {
            DeviceName=Marshal.PtrToStringUni(DirectGraphics.db_name(device));Vendor=Marshal.PtrToStringUni(DirectGraphics.db_vendor(device));Version=Marshal.PtrToStringUni(DirectGraphics.db_version(device));
            fallback=DirectGraphics.Check(DirectGraphics.db_texture(device,1,1,new byte[]{255,255,255,255},0));
            if(second){srTexture=DirectGraphics.Check(DirectGraphics.db_texture(device,1,1,new byte[]{0,0,0,255},0));}
            else {
                timeline=new Timeline(Path.Combine(assets,"timeline.xml"));var levels=new QualityAssets(assets);
                foreach(string file in Directory.GetFiles(assets,"*.json")){
                    string name=Path.GetFileNameWithoutExtension(file);Scene scene=Scene.Load(file);scenes.Add(name+".DAE",scene);
                    byte[] original=File.ReadAllBytes(Path.Combine(assets,name+".mesh"));
                    for(int i=0;i<scene.meshes.Length;i++){
                        SceneMesh mesh=scene.meshes[i];byte[] data=levels.Select(name+".DAE",i,quality.Id,mesh,original);GCHandle pin=GCHandle.Alloc(data,GCHandleType.Pinned);
                        try{mesh.Vao=(uint)DirectGraphics.Check(DirectGraphics.db_mesh(device,IntPtr.Add(pin.AddrOfPinnedObject(),mesh.offset),mesh.vertices,IntPtr.Add(pin.AddrOfPinnedObject(),mesh.indexOffset),mesh.indices));}finally{pin.Free();}
                    }
                }
                foreach(string file in Directory.GetFiles(assets).Where(f=>f.EndsWith(".png")||f.EndsWith(".jpg")))textures.Add(Path.GetFileName(file),LoadTexture(file));
                LoadedTriangles=scenes.Values.Sum(s=>s.meshes.Sum(m=>(long)m.indices/3));
            }
            root=NewTarget();
        }catch{Dispose();throw;}
    }
    int NewTarget(){return freeTargets.Count>0?freeTargets.Pop():DirectGraphics.Check(DirectGraphics.db_texture(device,quality.Width,quality.Height,null,1));}
    int LoadTexture(string file){
        using(Bitmap image=new Bitmap(file))using(Bitmap bitmap=new Bitmap(image.Width,image.Height,PixelFormat.Format32bppArgb)){
            using(Graphics graphics=Graphics.FromImage(bitmap))graphics.DrawImageUnscaled(image,0,0);
            BitmapData data=bitmap.LockBits(new Rectangle(0,0,bitmap.Width,bitmap.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
            try{byte[] bytes=new byte[bitmap.Width*bitmap.Height*4];for(int y=0;y<bitmap.Height;y++)Marshal.Copy(IntPtr.Add(data.Scan0,y*data.Stride),bytes,y*bitmap.Width*4,bitmap.Width*4);for(int i=0;i<bytes.Length;i+=4){byte red=bytes[i+2];bytes[i+2]=bytes[i];bytes[i]=red;}return DirectGraphics.Check(DirectGraphics.db_texture(device,bitmap.Width,bitmap.Height,bytes,0));}finally{bitmap.UnlockBits(data);}
        }
    }
    void Parameters(int kind){Array.Clear(constants,0,constants.Length);constants[16]=constants[17]=constants[18]=constants[19]=1;constants[20]=constants[21]=constants[26]=constants[28]=1;constants[35]=kind;}
    void Bind(int target,int clear,float[] color){DirectGraphics.Check(DirectGraphics.db_bind(device,target,clear,color));}
    bool Active(int id){
        if(id==0)return false;bool value;if(activity.TryGetValue(id,out value))return value;GraphNode node=timeline.Nodes[id];
        if(node.Type=="resource")value=true;else if(node.Type=="rendertarget"||node.Type=="output")value=node.Inputs.Any(c=>Active(c.Source));else{Effect effect=timeline.Effects[node.Effect];value=effect.Type=="layer"?Active(timeline.Input(node,"texInput")):effect.Active(time);}activity[id]=value;return value;
    }
    int Texture(int id){
        int image;if(cached.TryGetValue(id,out image))return image;GraphNode node=timeline.Nodes[id];if(node.Type=="resource")return textures[timeline.Resources[node.Resource]];
        int target;if(!targets.TryGetValue(id,out target)){target=NewTarget();targets.Add(id,target);}Bind(target,3,black);DrawNode(id,target);cached.Add(id,target);return target;
    }
    void DrawNode(int id,int target){
        if(!Active(id))return;GraphNode node=timeline.Nodes[id];if(node.Type=="rendertarget"||node.Type=="output"){foreach(Connection connection in node.Inputs)DrawNode(connection.Source,target);return;}if(node.Type!="effect")return;Effect effect=timeline.Effects[node.Effect];
        if(effect.Type=="assimprenderer"){
            Scene scene=scenes[timeline.ResourceInput(node,"Assimp Scene")];float[] vp=scene.Evaluate(effect.Value("time",time,0),(int)effect.Value("camera",time,0),(float)quality.Width/quality.Height);
            bool clear=timeline.ResourceInput(node,"DX Shader")=="ShaderClear.fx";Bind(target,clear?3:2,clear?white:black);
            foreach(SceneNode item in scene.nodes)if(item.meshes.Length>0){float[] matrix=Mat.Mul(vp,item.World);foreach(int index in item.meshes){SceneMesh mesh=scene.meshes[index];Parameters(0);Array.Copy(matrix,constants,16);Array.Copy(scene.Colors[mesh.material],0,constants,16,4);DirectGraphics.Check(DirectGraphics.db_draw(device,(int)mesh.Vao,constants,fallback,fallback));DrawCalls++;Triangles+=mesh.indices/3;}}
        }else if(effect.Type=="layer"){
            float alpha=effect.Value("fAlpha",time,1);if(alpha<=0)return;int image=Texture(timeline.Input(node,"texInput"));Bind(target,0,black);Parameters(1);
            constants[20]=effect.Value("fScaleX",time,1);constants[21]=effect.Value("fScaleY",time,1);constants[22]=effect.Value("fPosX",time,0);constants[23]=effect.Value("fPosY",time,0);
            constants[24]=effect.Value("fOffsetX",time,0);constants[25]=-effect.Value("fOffsetY",time,0);constants[26]=effect.Value("fZoom",time,1);
            constants[28]=alpha;constants[29]=timeline.ResourceInput(node,"DX Shader")=="NoiseCrunch.fx"?1:0;constants[30]=effect.Value("fWidth",time,0);constants[31]=effect.Value("fTime",time,0);
            DirectGraphics.Check(DirectGraphics.db_draw(device,1,constants,image,textures["noise-pixelate.png"]));DrawCalls++;Triangles+=2;
        }
    }
    public void Render(double seconds,SecondRealitySession session){
        DirectGraphics.Check(DirectGraphics.db_begin(device));DrawCalls=Triangles=0;
        if(secondReality){
            if(session!=null&&srSerial!=session.SourceFrames){DirectGraphics.Check(DirectGraphics.db_update(device,srTexture,session.Width,session.Height,session.Pixels));srSerial=session.SourceFrames;}
            Bind(root,3,black);Parameters(quality.Id==0?2:3);DirectGraphics.Check(DirectGraphics.db_draw(device,1,constants,srTexture,fallback));DrawCalls=1;Triangles=2;
        }else{
            time=(seconds+Timeline.ClockOffset)*1000;activity.Clear();cached.Clear();
            foreach(int id in targets.Keys.Where(k=>!Active(k)).ToArray()){freeTargets.Push(targets[id]);targets.Remove(id);}
            Bind(root,3,black);DrawNode(10,root);
            foreach(int id in targets.Keys.Where(k=>!cached.ContainsKey(k)).ToArray()){freeTargets.Push(targets[id]);targets.Remove(id);}
        }
    }
    public void Present(){DirectGraphics.Check(DirectGraphics.db_begin(device));DirectGraphics.Check(DirectGraphics.db_present(device,root,secondReality&&quality.Id==0?1:0));}
    public bool SetVsync(bool enabled){bool ok=DirectGraphics.db_vsync(device,enabled?1:0)!=0;Vsync=enabled&&ok;return ok;}
    public void ResetSecondReality(){srSerial=-1;DirectGraphics.Check(DirectGraphics.db_update(device,srTexture,1,1,new byte[]{0,0,0,255}));}
    public void Finish(){DirectGraphics.Check(DirectGraphics.db_finish(device));}
    public void Capture(string path){
        byte[] pixels=new byte[quality.Width*quality.Height*4];DirectGraphics.Check(DirectGraphics.db_capture(device,root,pixels));for(int i=0;i<pixels.Length;i+=4){byte red=pixels[i+2];pixels[i+2]=pixels[i];pixels[i]=red;}
        using(Bitmap bitmap=new Bitmap(quality.Width,quality.Height,PixelFormat.Format32bppArgb)){BitmapData data=bitmap.LockBits(new Rectangle(0,0,bitmap.Width,bitmap.Height),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);try{for(int y=0;y<bitmap.Height;y++)Marshal.Copy(pixels,y*bitmap.Width*4,IntPtr.Add(data.Scan0,y*data.Stride),bitmap.Width*4);}finally{bitmap.UnlockBits(data);}bitmap.Save(path,ImageFormat.Png);}
    }
    public void Dispose(){if(device!=IntPtr.Zero){DirectGraphics.db_destroy(device);device=IntPtr.Zero;}}
}
}
