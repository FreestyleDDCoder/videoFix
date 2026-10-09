package com.desaysv.videofix

internal object UntruncNative {
    init {
        System.loadLibrary("untrunc")
    }

    @JvmStatic
    external fun nativeRepair(
        refPath: String,
        brokenPath: String,
        dstPath: String,
        onProgress: OnProgressListener?,
        onLog: OnLogListener?,
    ): Int

    @JvmStatic
    external fun nativeVersion(): String
}
