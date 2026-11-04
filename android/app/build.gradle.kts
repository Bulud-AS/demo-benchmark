plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
    id("com.google.devtools.ksp")
}
android {
    namespace = "io.github.buludas.demobenchmark"
    compileSdk = 36
    ndkVersion = "28.2.13676358"
    defaultConfig {
        applicationId = "io.github.buludas.demobenchmark"
        minSdk = 29
        targetSdk = 36
        versionCode = 5
        versionName = "0.1.3-beta"
        ndk { abiFilters += (project.findProperty("qaAbi")?.toString() ?: "arm64-v8a") }
        externalNativeBuild { cmake { cppFlags += listOf("-std=c++20", "-O2", "-fno-strict-aliasing"); arguments += "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON" } }
    }
    buildTypes {
        release { isMinifyEnabled = true; proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro") }
        debug { applicationIdSuffix = ".qa" }
    }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget = "17" }
    buildFeatures { compose = true; buildConfig = true }
    externalNativeBuild { cmake { path = file("src/main/cpp/CMakeLists.txt"); version = "3.22.1" } }
    sourceSets["main"].assets.srcDirs("../../assets", "src/main/assets", "../../notices")
    sourceSets["main"].assets.srcDir(layout.buildDirectory.dir("generated/vulkan-assets"))
    androidResources { noCompress += listOf("mesh", "wav", "spv") }
    packaging { resources.excludes += setOf("META-INF/LICENSE*", "META-INF/NOTICE*") }
}
val compileBenchShaders by tasks.registering {
    val source = file("src/main/assets/shaders")
    inputs.dir(source)
    val output = layout.buildDirectory.dir("generated/vulkan-assets/shaders")
    outputs.dir(output)
    doLast {
        val host = when { System.getProperty("os.name").startsWith("Windows") -> "windows-x86_64"; System.getProperty("os.name").startsWith("Mac") -> "darwin-x86_64"; else -> "linux-x86_64" }
        val compiler = android.sdkDirectory.resolve("ndk/${android.ndkVersion}/shader-tools/$host/glslc" + if(host.startsWith("windows")) ".exe" else "")
        val common = source.resolve("common.glsl").readText()
        output.get().asFile.mkdirs()
        val staging = layout.buildDirectory.dir("shader-source").get().asFile.apply { mkdirs() }
        for(name in listOf("scene.vert","scene.frag","quad.vert","quad.frag")) {
            val input = staging.resolve(name)
            input.writeText("#version 450\n" + common + "\n" + source.resolve(name).readText())
            exec { commandLine(compiler.absolutePath, "--target-env=vulkan1.1", "-fshader-stage=" + if(name.endsWith("vert")) "vert" else "frag", input.absolutePath, "-o", output.get().file("$name.spv").asFile.absolutePath) }
        }
    }
}
tasks.named("preBuild").configure { dependsOn(compileBenchShaders) }
dependencies {
    implementation("androidx.activity:activity-compose:1.10.1")
    implementation("androidx.compose.ui:ui:1.8.3")
    implementation("androidx.compose.ui:ui-tooling-preview:1.8.3")
    implementation("androidx.compose.material3:material3:1.4.0-alpha18")
    implementation("androidx.compose.material:material-icons-extended:1.7.8")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.1")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.9.1")
    implementation("androidx.datastore:datastore-preferences:1.1.7")
    implementation("androidx.room:room-runtime:2.7.2")
    implementation("androidx.room:room-ktx:2.7.2")
    ksp("androidx.room:room-compiler:2.7.2")
    debugImplementation("androidx.compose.ui:ui-tooling:1.8.3")
}
