package com.desaysv.videofix

/**
 * 修复过程回调。引擎在 native 线程回调，回调内请勿做 UI 操作，
 * 如需更新 UI 请自行切换到主线程。
 *
 * Java 侧可用 lambda（需 language level 8+）或匿名类实现。
 */
fun interface OnProgressListener {
    /** percent: 0..100 */
    fun onProgress(percent: Int)
}

/**
 * 日志回调。引擎在 native 线程回调。
 */
fun interface OnLogListener {
    /** level 见 [Videofix.LEVEL_*]，msg 为 UTF-8 文本 */
    fun onLog(level: Int, msg: String)
}

/**
 * 异步修复完成回调，code 为 [Videofix] 的结果码（[Videofix.OK] 表示成功）。
 */
fun interface OnCompleteListener {
    fun onComplete(code: Int)
}