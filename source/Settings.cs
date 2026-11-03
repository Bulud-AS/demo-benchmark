using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;

namespace DemoBenchmark {
public enum DemoKind { Median, SecondReality }
public enum RenderApi { OpenGL, Direct3D11, Vulkan }
public sealed class QualityPreset {
    public int Id, Width, Height, Samples, Subdivision;
    public string Name;
    public override string ToString() { return Name; }
    public static readonly QualityPreset[] SecondReality = {
        new QualityPreset { Id=0, Name="原画質", Width=640, Height=480, Samples=1, Subdivision=0 },
        new QualityPreset { Id=1, Name="高画質（補間）", Width=1440, Height=1080, Samples=1, Subdivision=0 }
    };
    public static readonly QualityPreset[] All = {
        new QualityPreset { Id=0, Name="最低", Width=960, Height=540, Samples=1, Subdivision=0 },
        new QualityPreset { Id=1, Name="低", Width=1280, Height=720, Samples=1, Subdivision=1 },
        new QualityPreset { Id=2, Name="中", Width=1280, Height=720, Samples=4, Subdivision=2 },
        new QualityPreset { Id=3, Name="高", Width=1920, Height=1080, Samples=4, Subdivision=3 },
        new QualityPreset { Id=4, Name="最高", Width=2560, Height=1440, Samples=4, Subdivision=4 }
    };
}
public sealed class LodEntry { public string scene; public int mesh; public LodLevel[] levels; }
public sealed class LodLevel { public string file; public int offset,indexOffset,vertices,indices,material; }
public sealed class QualityAssets {
    readonly Dictionary<string,LodEntry> entries = new Dictionary<string,LodEntry>();
    readonly Dictionary<string,byte[]> files = new Dictionary<string,byte[]>();
    readonly string folder;
    public QualityAssets(string assets) {
        folder=Path.Combine(assets,"quality");
        string path=Path.Combine(folder,"manifest.json");
        if(!File.Exists(path))return;
        foreach(LodEntry e in new JavaScriptSerializer { MaxJsonLength=int.MaxValue }.Deserialize<LodEntry[]>(File.ReadAllText(path)))
            entries.Add(e.scene+"/"+e.mesh,e);
    }
    public byte[] Select(string scene,int mesh,int quality,SceneMesh target,byte[] original) {
        LodEntry e;
        if(!entries.TryGetValue(scene+"/"+mesh,out e))return original;
        LodLevel l=e.levels[quality];
        target.offset=l.offset;target.indexOffset=l.indexOffset;target.vertices=l.vertices;target.indices=l.indices;
        byte[] bytes;
        if(!files.TryGetValue(l.file,out bytes)) { bytes=File.ReadAllBytes(Path.Combine(folder,l.file)); files.Add(l.file,bytes); }
        return bytes;
    }
}
public static class ApiNames {
    public static string Display(RenderApi api) { return api==RenderApi.OpenGL?"OpenGL 3.3":api==RenderApi.Direct3D11?"Direct3D11 (ANGLE)":"Vulkan (ANGLE)"; }
}
}
