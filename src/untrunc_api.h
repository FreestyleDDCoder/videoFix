/*
	Untrunc - untrunc_api.h

	Library entry points for embedding untrunc into other applications
	(e.g. Android via JNI). This is a thin, exception-free C wrapper around
	the core Mp4::parseOk() + Mp4::repair() pipeline.

	NOT thread-safe by itself: the underlying code uses global state.
	Serialise calls (see untrunc_repair_locked / caller-side mutex).

	Untrunc is GPL software; you can freely distribute,
	redistribute, modify & use under the terms of the GNU General
	Public License; either version 2 or its successor.
*/

#ifndef UNTRUNC_API_H
#define UNTRUNC_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* error codes returned by the API */
enum UntruncStatus {
	UNTRUNC_OK = 0,          /* repaired file written (or already existed) */
	UNTRUNC_ERR_ARGS = 1,    /* null/empty arguments */
	UNTRUNC_ERR_REF = 2,     /* reference (ok) video could not be parsed */
	UNTRUNC_ERR_BROKEN = 3,  /* broken video could not be opened/scanned */
	UNTRUNC_ERR_OUTPUT = 4,  /* destination not writable */
	UNTRUNC_ERR_INTERNAL = 5 /* unexpected internal error (assert/throw) */
};

/* log levels mirrored from LogMode (common.h) */
enum UntruncLogLevel {
	UNTRUNC_LOG_ERROR = 0,
	UNTRUNC_LOG_INFO  = 1,
	UNTRUNC_LOG_WARN  = 2,
	UNTRUNC_LOG_DEBUG = 3
};

/* progress callback: percent 0..100 (double precision truncated) */
typedef void (*untrunc_progress_cb)(int percent, void* user_data);
/* log callback: msg is UTF-8, valid only during the call */
typedef void (*untrunc_log_cb)(int level, const char* msg, void* user_data);

/*
	Repair a truncated MP4/MOV using a healthy reference video from the
	same device.

	ref_path    healthy reference video
	broken_path truncated/broken video
	dst_path    output file path (with extension, e.g. ".../out.mp4").
	            Parent directory must exist and be writable.
	on_progress optional progress callback (0-100), may be NULL
	on_log      optional log callback, may be NULL
	user_data   opaque pointer passed back to the callbacks

	Returns UNTRUNC_OK on success; the repaired file is at dst_path.
*/
int untrunc_repair(const char* ref_path,
                   const char* broken_path,
                   const char* dst_path,
                   untrunc_progress_cb on_progress,
                   untrunc_log_cb on_log,
                   void* user_data);

/* library version string, e.g. "untrunc 1.x.x (ffmpeg n7.x)" */
const char* untrunc_version(void);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // UNTRUNC_API_H