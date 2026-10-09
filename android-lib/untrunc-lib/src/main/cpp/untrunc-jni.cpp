/*
	Untrunc - untrunc-jni.cpp

	JNI bridge between Kotlin UntruncNative and untrunc_api.h.
	Threading: UntruncNative serialises calls; no locking needed here.
	The engine calls back on its own thread, so we attach/detach as needed.

	Untrunc is GPL software; you can freely distribute,
	redistribute, modify & use under the terms of the GNU General
	Public License; either version 2 or its successor.
*/

#include <jni.h>

#include "untrunc_api.h"

namespace {

JavaVM* g_vm = nullptr;
jmethodID g_mid_on_progress = nullptr;
jmethodID g_mid_on_log = nullptr;

JNIEnv* getEnv(bool& attached) {
	attached = false;
	JNIEnv* env = nullptr;
	if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK)
		return env;
	g_vm->AttachCurrentThread(&env, nullptr);
	attached = true;
	return env;
}

// per-call context passed through the engine's user_data pointer
struct CallbackCtx {
	jobject progress_ref;  // global ref to OnProgressListener or null
	jobject log_ref;       // global ref to OnLogListener or null
};

void progress_cb(int percent, void* user_data) {
	auto ctx = static_cast<CallbackCtx*>(user_data);
	if (!ctx || !ctx->progress_ref || !g_mid_on_progress) return;
	bool attached;
	JNIEnv* env = getEnv(attached);
	env->CallVoidMethod(ctx->progress_ref, g_mid_on_progress,
	                    static_cast<jint>(percent));
	if (env->ExceptionCheck()) env->ExceptionClear();
	if (attached) g_vm->DetachCurrentThread();
}

void log_cb(int level, const char* msg, void* user_data) {
	auto ctx = static_cast<CallbackCtx*>(user_data);
	if (!ctx || !ctx->log_ref || !g_mid_on_log) return;
	bool attached;
	JNIEnv* env = getEnv(attached);
	jstring jmsg = env->NewStringUTF(msg ? msg : "");
	env->CallVoidMethod(ctx->log_ref, g_mid_on_log,
	                    static_cast<jint>(level), jmsg);
	if (env->ExceptionCheck()) env->ExceptionClear();
	env->DeleteLocalRef(jmsg);
	if (attached) g_vm->DetachCurrentThread();
}

}  // namespace

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
	g_vm = vm;
	JNIEnv* env;
	if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
		return JNI_ERR;

	// com.desaysv.videofix.OnProgressListener / OnLogListener
	jclass progress = env->FindClass("com/desaysv/videofix/OnProgressListener");
	if (!progress) return JNI_ERR;
	g_mid_on_progress = env->GetMethodID(progress, "onProgress", "(I)V");
	env->DeleteLocalRef(progress);

	jclass log = env->FindClass("com/desaysv/videofix/OnLogListener");
	if (!log) return JNI_ERR;
	g_mid_on_log = env->GetMethodID(log, "onLog", "(ILjava/lang/String;)V");
	env->DeleteLocalRef(log);

	if (!g_mid_on_progress || !g_mid_on_log) return JNI_ERR;
	return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jint
Java_com_desaysv_videofix_UntruncNative_nativeRepair(
		JNIEnv* env, jclass, jstring j_ref, jstring j_broken, jstring j_dst,
		jobject j_on_progress, jobject j_on_log) {
	if (!j_ref || !j_broken || !j_dst) return UNTRUNC_ERR_ARGS;

	const char* ref = env->GetStringUTFChars(j_ref, nullptr);
	const char* broken = env->GetStringUTFChars(j_broken, nullptr);
	const char* dst = env->GetStringUTFChars(j_dst, nullptr);
	if (!ref || !broken || !dst) {
		if (ref) env->ReleaseStringUTFChars(j_ref, ref);
		if (broken) env->ReleaseStringUTFChars(j_broken, broken);
		if (dst) env->ReleaseStringUTFChars(j_dst, dst);
		return UNTRUNC_ERR_ARGS;
	}

	CallbackCtx ctx{ j_on_progress ? env->NewGlobalRef(j_on_progress) : nullptr,
	                 j_on_log ? env->NewGlobalRef(j_on_log) : nullptr };

	jint result = untrunc_repair(
	    ref, broken, dst,
	    ctx.progress_ref ? &progress_cb : nullptr,
	    ctx.log_ref ? &log_cb : nullptr,
	    &ctx);

	if (ctx.progress_ref) env->DeleteGlobalRef(ctx.progress_ref);
	if (ctx.log_ref) env->DeleteGlobalRef(ctx.log_ref);

	env->ReleaseStringUTFChars(j_ref, ref);
	env->ReleaseStringUTFChars(j_broken, broken);
	env->ReleaseStringUTFChars(j_dst, dst);
	return result;
}

extern "C" JNIEXPORT jstring
Java_com_desaysv_videofix_UntruncNative_nativeVersion(JNIEnv* env, jclass) {
	return env->NewStringUTF(untrunc_version());
}
