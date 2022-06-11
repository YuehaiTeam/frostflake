#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>
#include <string>
const int idleInterval = 60;
const int idleStby = 120;
const int idleNotf = 300;
const int idleQuit = 360;
time_t lastActivity = 0;
bool idleTimerRunning = true;
boost::asio::io_context tmr_io_context;
boost::asio::deadline_timer idleTimer(tmr_io_context, boost::posix_time::seconds(idleInterval));

bool stopTrack();
void endUiThread();
int ws_count_connection();
int ws_count_evs_connection();
void sendNotify(std::wstring msg, std::wstring title);

void notIdle() {
    lastActivity = time(NULL);
}
void tmr_tick(const boost::system::error_code &ec) {
    if (!idleTimerRunning) {
        return;
    }
    time_t now = time(NULL);
    int wsCount = ws_count_connection();
    int evsCount = ws_count_evs_connection();
    if (wsCount == 0 && evsCount == 0) {
        if (now - lastActivity > idleQuit) {
            endUiThread();
        } else if (now - lastActivity > idleNotf) {
            sendNotify(L"您已长时间没有使用椰羊插件", L"插件将在一分钟后自动退出，如需使用，请再次启动");
        } else if (now - lastActivity > idleStby) {
            // stopTrack();
        }
    }
    idleTimer.expires_from_now(boost::posix_time::seconds(idleInterval));
    idleTimer.async_wait(tmr_tick);
}
void tmr_run() {
    lastActivity = time(nullptr);
    idleTimer.expires_from_now(boost::posix_time::seconds(idleInterval));
    idleTimer.async_wait(tmr_tick);
    tmr_io_context.run();
}
void tmr_stop() {
    idleTimerRunning = false;
    idleTimer.cancel();
    tmr_io_context.stop();
}