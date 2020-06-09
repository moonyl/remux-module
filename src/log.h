
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
void log_packet(const AVFormatContext* fmt_ctx, const AVPacket* pkt, const char* tag);
char* av_err2str_wrap(int num);