#pragma once
/* Diagnostic log on the SD: sdmc:/3ds/RadioNeon/net.log (rotated to net.old.log at 256 KB).
   Thread safe; every line gets a date/time stamp. */
void netlog_init(void);
void netlog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
