/*
	Untrunc - untrunc_api.cpp

	Library entry point wrapping Mp4::parseOk() + Mp4::repair().

	Survives the CLI heritage by leveraging existing switches:
	  - g_is_gui=true      -> logg(ET) throws std::runtime_error instead of exit(1)
	                          (see common.h _logg)
	  - g_interactive=false-> hitEnterToContinue() never calls getchar()
	  - g_skip_existing    -> kept false so Mp4::repair() can never exit(0)
	                          at its alreadyRepaired() check (mp4.cpp)
	  - g_dst_path         -> when not a directory, rewriteDestination() uses it
	                          verbatim as the output file path (mp4.cpp)
	  - g_onProgress       -> trampolined to the caller's callback; requires
	                          g_log_mode == I to be emitted (mp4.cpp repair loop)

	All touched globals are saved/restored so consecutive calls behave
	independently. Calls must still be serialised by the caller (global
	state inside the repair engine).

	Untrunc is GPL software; you can freely distribute,
	redistribute, modify & use under the terms of the GNU General
	Public License; either version 2 or its successor.
*/

#include "untrunc_api.h"

#include <cstring>
#include <string>

#include "common.h"
#include "mp4.h"

namespace {

untrunc_progress_cb g_api_progress = nullptr;
untrunc_log_cb g_api_log = nullptr;
void* g_api_user = nullptr;

void progressTrampoline(int percent) {
	if (g_api_progress) g_api_progress(percent, g_api_user);
}

void apiLog(int level, const std::string& msg) {
	if (g_api_log) g_api_log(level, msg.c_str(), g_api_user);
}

// RAII: configure globals for the call, restore the previous state afterwards
struct GlobalGuard {
	LogMode log_mode;
	bool interactive, is_gui, skip_existing;
	std::string dst_path;
	void (*on_progress)(int);

	GlobalGuard() {
		log_mode = g_log_mode;
		interactive = g_interactive;
		is_gui = g_is_gui;
		skip_existing = g_skip_existing;
		dst_path = g_dst_path;
		on_progress = g_onProgress;
	}
	~GlobalGuard() {
		g_log_mode = log_mode;
		g_interactive = interactive;
		g_is_gui = is_gui;
		g_skip_existing = skip_existing;
		g_dst_path = dst_path;
		g_onProgress = on_progress;
		g_num_w2 = 0;
		g_api_progress = nullptr;
		g_api_log = nullptr;
		g_api_user = nullptr;
	}
};

}  // namespace

extern "C" int untrunc_repair(const char* ref_path,
                              const char* broken_path,
                              const char* dst_path,
                              untrunc_progress_cb on_progress,
                              untrunc_log_cb on_log,
                              void* user_data) {
	if (!ref_path || !*ref_path || !broken_path || !*broken_path ||
	    !dst_path || !*dst_path)
		return UNTRUNC_ERR_ARGS;

	GlobalGuard guard;  // must live until the end of the call

	g_log_mode = LogMode::I;   // required so outProgress() is emitted
	g_interactive = false;     // no getchar() prompts
	g_is_gui = true;           // ET-level errors throw instead of exit(1)
	g_skip_existing = false;   // never take the exit(0) shortcut
	g_dst_path = dst_path;     // exact output file path (not a dir)
	g_num_w2 = 0;

	g_api_progress = on_progress;
	g_api_log = on_log;
	g_api_user = user_data;
	g_onProgress = on_progress ? &progressTrampoline : nullptr;

	if (!FileRead::alreadyExists(ref_path)) {
		apiLog(UNTRUNC_LOG_ERROR, "reference video does not exist");
		return UNTRUNC_ERR_REF;
	}
	if (!FileRead::alreadyExists(broken_path)) {
		apiLog(UNTRUNC_LOG_ERROR, "broken video does not exist");
		return UNTRUNC_ERR_BROKEN;
	}

	Mp4 mp4;
	g_mp4 = &mp4;  // engine-wide back-reference (was set in main())

	// stage 1: learn from the healthy reference video
	try {
		mp4.parseOk(ref_path);
	} catch (const std::exception& e) {
		apiLog(UNTRUNC_LOG_ERROR, e.what());
		return UNTRUNC_ERR_REF;
	} catch (const char* e) {
		apiLog(UNTRUNC_LOG_ERROR, e);
		return UNTRUNC_ERR_REF;
	} catch (std::string e) {
		apiLog(UNTRUNC_LOG_ERROR, e.c_str());
		return UNTRUNC_ERR_REF;
	}

	// stage 2: scan the broken file and write dst_path
	try {
		mp4.repair(broken_path);
	} catch (const std::exception& e) {
		apiLog(UNTRUNC_LOG_ERROR, e.what());
		return UNTRUNC_ERR_INTERNAL;
	} catch (const char* e) {
		apiLog(UNTRUNC_LOG_ERROR, e);
		return UNTRUNC_ERR_INTERNAL;
	} catch (std::string e) {
		apiLog(UNTRUNC_LOG_ERROR, e.c_str());
		return UNTRUNC_ERR_INTERNAL;
	}

	if (!FileRead::alreadyExists(dst_path)) {
		apiLog(UNTRUNC_LOG_ERROR, "repair finished but output is missing");
		return UNTRUNC_ERR_OUTPUT;
	}
	return UNTRUNC_OK;
}

extern "C" const char* untrunc_version(void) {
	return g_version_str.c_str();
}