package com.desaysv.videofix

import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Untrunc 视频修复门面。
 *
 * 用法（Kotlin）：
 * ```kotlin
 * val code = Videofix.repair(
 *     refPath = "/sdcard/ref_ok.mp4",
 *     brokenPath = "/sdcard/broken.mp4",
 *     outPath = "/sdcard/out/fixed.mp4",
 *     onProgress = { percent -> ... },
 *     onLog = { level, msg -> ... },
 * )
 * ```
 *
 * 用法（Java，lambda 需要 language level 8+）：
 * ```java
 * int code = Videofix.repair(ref, broken, out,
 *         percent -> { ... },            // OnProgressListener
 *         (level, msg) -> { ... });      // OnLogListener
 * Videofix.repairAsync(ref, broken, out, null, null,
 *         code -> { ... });              // OnCompleteListener
 * ```
 *
 * 引擎内部使用全局状态（CLI 遗留），因此所有修复任务串行执行：
 * 同步版并发调用会抛 [IllegalStateException]，异步版并发调用会静默失败并回调 [ERR_INTERNAL]。
 */
object Videofix {

    /** 日志级别：错误 */
    const val LEVEL_ERROR = 0
    /** 日志级别：关键信息 */
    const val LEVEL_INFO = 1
    /** 日志级别：警告 */
    const val LEVEL_WARN = 2
    /** 日志级别：调试 */
    const val LEVEL_DEBUG = 3

    /** 成功：repair 的 outPath 已生成修复后视频 */
    const val OK = 0
    /** 参数错误（路径为空等） */
    const val ERR_ARGS = 1
    /** 参考视频不存在或无法解析 */
    const val ERR_REF = 2
    /** 损坏视频不存在或无法扫描 */
    const val ERR_BROKEN = 3
    /** 修复结束但输出文件缺失（磁盘满/路径不可写等） */
    const val ERR_OUTPUT = 4
    /** 引擎内部错误（异常/断言），或已有任务在执行（异步版） */
    const val ERR_INTERNAL = 5

    private val running = AtomicBoolean(false)
    // daemon 单线程：修复任务互斥，进程退出不阻塞
    private val executor: ExecutorService =
        Executors.newSingleThreadExecutor { r -> Thread(r, "untrunc-repair").apply { isDaemon = true } }

    /** 引擎版本串，如 "untrunc 1.x.x (ffmpeg n7.x)" */
    fun version(): String = UntruncNative.nativeVersion()

    /**
     * 同步修复（阻塞调用线程直到完成）。同一时刻仅允许一个修复任务。
     *
     * @param refPath    同设备录制的健康参考视频
     * @param brokenPath 被截断/损坏的视频
     * @param outPath    输出文件路径（含扩展名），父目录必须存在且可写
     * @param onProgress 可选进度回调（0..100），native 线程触发
     * @param onLog      可选日志回调，native 线程触发
     * @return [OK] 表示成功；其余见 [ERR_ARGS] 等错误码
     * @throws IllegalStateException 已有修复任务在执行
     */
    @JvmOverloads
    fun repair(
        refPath: String,
        brokenPath: String,
        outPath: String,
        onProgress: OnProgressListener? = null,
        onLog: OnLogListener? = null,
    ): Int {
        if (!running.compareAndSet(false, true)) {
            throw IllegalStateException("another repair is in progress")
        }
        return try {
            UntruncNative.nativeRepair(refPath, brokenPath, outPath, onProgress, onLog)
        } finally {
            running.set(false)
        }
    }

    /**
     * 异步修复，内部在单线程 executor 上排队执行，不阻塞调用线程。
     * [onProgress]/[onLog]/[onComplete] 均在后台线程触发。
     *
     * 若已有任务在执行，本次提交不会排队而是立即回调 [ERR_INTERNAL]。
     */
    @JvmOverloads
    fun repairAsync(
        refPath: String,
        brokenPath: String,
        outPath: String,
        onProgress: OnProgressListener? = null,
        onLog: OnLogListener? = null,
        onComplete: OnCompleteListener? = null,
    ) {
        executor.submit {
            val code = try {
                repair(refPath, brokenPath, outPath, onProgress, onLog)
            } catch (e: IllegalStateException) {
                ERR_INTERNAL
            }
            onComplete?.onComplete(code)
        }
    }
}
