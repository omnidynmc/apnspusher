#include "config.h"

#include <string>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <openframe/openframe.h>
#include <stomp/StompHeaders.h>
#include <stomp/StompFrame.h>
#include <stomp/Stomp.h>
#include <apns/apns.h>

#include <App.h>
#include <APNS.h>
#include <Worker.h>
#include <Store.h>
#include <MemcachedController.h>

namespace apnspusher {
  using namespace openframe::loglevel;

  const int Worker::kDefaultStompPrefetch		= 1024;
  const time_t Worker::kDefaultStatsInterval		= 3600;
  const time_t Worker::kDefaultMemcachedExpire		= 3600;
  const char *Worker::kDefaultStompDestNotifyMessages	= "/topic/notify.aprs.messages";

  Worker::Worker(const thread_id_t thread_id,
                 const std::string &stomp_hosts,
                 const std::string &stomp_login,
                 const std::string &stomp_passcode,
                 const std::string &memcached_host,
                 const std::string &db_host,
                 const std::string &db_user,
                 const std::string &db_pass,
                 const std::string &db_database)
         : openframe::LogObject(thread_id),
           _stomp_hosts(stomp_hosts),
           _stomp_login(stomp_login),
           _stomp_passcode(stomp_passcode),
           _memcached_host(memcached_host),
           _db_host(db_host),
           _db_user(db_user),
           _db_pass(db_pass),
           _db_database(db_database) {

    _store = NULL;
    _stomp = NULL;
    _apns = NULL;
    _connected = false;
    _console = false;
    _no_send = false;

    _stomp_dest_notify_msgs = kDefaultStompDestNotifyMessages;

    init_stats(_stats, true);
    init_stompstats(_stompstats, true);
    _stats.report_interval = 60;
    _stompstats.report_interval = 5;
  } // Worker::Worker

  Worker::~Worker() {
    onDestroyStats();

    _apns->stop();
    if (_apns) delete _apns;

    if (_store) delete _store;
    if (_stomp) delete _stomp;

  } // Worker:~Worker

  void Worker::init() {
    try {
      stomp::StompHeaders *headers = new stomp::StompHeaders("openstomp.prefetch",
                                                             openframe::stringify<int>(kDefaultStompPrefetch)
                                                            );
      headers->add_header("heart-beat", "0,5000");
      _stomp = new stomp::Stomp(_stomp_hosts,
                                _stomp_login,
                                _stomp_passcode,
                                headers);

      _store = new Store(thread_id(),
                         _db_host,
                         _db_user,
                         _db_pass,
                         _db_database,
                         _memcached_host,
                         kDefaultMemcachedExpire,
                         kDefaultStatsInterval);
      _store->replace_stats( stats(), "");
      _store->set_elogger( elogger(), elog_name() );
      _store->init();

      _apns = new APNS(app->cfg->get_int("app.apns.push", 1),
                       app->cfg->get_bool("app.apns.feedback.enable", false)
                      );
      _apns->elogger( elogger(), elog_name() );

      _apns->set_cert(app->cfg->get_string("app.apns.ssl.cert"),
                      app->cfg->get_string("app.apns.ssl.key")
                     );

      _apns->set_push(app->cfg->get_string("app.apns.push.host"),
                      app->cfg->get_int("app.apns.push.port"),
                      app->cfg->get_int("app.apns.push.timeout")
                     );

      _apns->set_feedback(app->cfg->get_string("app.apns.feedback.host"),
                          app->cfg->get_int("app.apns.feedback.port"),
                          app->cfg->get_int("app.apns.feedback.interval")
                         );

      _apns->start();
    } // try
    catch(std::bad_alloc xa) {
      assert(false);
    } // catch
  } // Worker::init

  void Worker::init_stats(obj_stats_t &stats, const bool startup) {
    stats.connects = 0;
    stats.disconnects = 0;
    stats.packets = 0;
    stats.frames_in = 0;
    stats.frames_out = 0;

    stats.last_report_at = time(NULL);
    if (startup) stats.created_at = time(NULL);
  } // Worker::init_stats

  void Worker::init_stompstats(obj_stompstats_t &stats, const bool startup) {
    memset(&stats.aprs_stats, '\0', sizeof(aprs_stats_t) );

    stats.last_report_at = time(NULL);
    if (startup) stats.created_at = time(NULL);
  } // Worker::init_stompstats

  void Worker::onDescribeStats() {
    describe_stat("num.frames.out", "worker"+thread_id_str()+"/num frames out", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_stat("num.frames.in", "worker"+thread_id_str()+"/num frames in", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_stat("num.bytes.out", "worker"+thread_id_str()+"/num bytes out", openstats::graphTypeCounter, openstats::dataTypeInt);
    describe_stat("num.bytes.in", "worker"+thread_id_str()+"/num bytes in", openstats::graphTypeCounter, openstats::dataTypeInt);
  } // Worker::onDescribeStats

  void Worker::onDestroyStats() {
    destroy_stat("*");
  } // Worker::onDestroyStats

  void Worker::try_stats() {
    try_stompstats();

    if (_stats.last_report_at > time(NULL) - _stats.report_interval) return;

    int diff = time(NULL) - _stats.last_report_at;
    double pps = double(_stats.packets) / diff;
    double fps_in = double(_stats.frames_in) / diff;
    double fps_out = double(_stats.frames_out) / diff;

    TLOG(LogNotice, << "Stats packets " << _stats.packets
                    << ", pps " << pps << "/s"
                    << ", frames in " << _stats.frames_in
                    << ", fps in " << fps_in << "/s"
                    << ", frames out " << _stats.frames_out
                    << ", fps out " << fps_out << "/s"
                    << ", next in " << _stats.report_interval
                    << ", connect attempts " << _stats.connects
                    << "; " << _stomp->connected_to()
                    << std::endl);

    init_stats(_stats);
    _stats.last_report_at = time(NULL);
  } // Worker::try_stats

  void Worker::try_stompstats() {
    if (_stompstats.last_report_at > time(NULL) - _stompstats.report_interval) return;

    init_stompstats(_stompstats);
  } // Worker::try_stompstats

  bool Worker::run() {
    try_stats();
    _store->try_stats();

    /**********************
     ** Check Connection **
     **********************/
    if (!_connected) {
      ++_stats.connects;
      bool ok = _stomp->subscribe(_stomp_dest_notify_msgs, "1");
      if (!ok) {
        TLOG(LogInfo, << "not connected, retry in 2 seconds; " << _stomp->last_error() << std::endl);
        return false;
      } // if
      _connected = true;
      TLOG(LogNotice, << "Connected to " << _stomp->connected_to() << std::endl);
    } // if

    stomp::StompFrame *frame;
    bool ok = false;

    try {
      ok = _stomp->next_frame(frame);
    } // try
    catch(stomp::Stomp_Exception ex) {
      TLOG(LogWarn, << "ERROR: " << ex.message() << std::endl);
      _connected = false;
      ++_stats.disconnects;
      return false;
    } // catch

    if (!ok) return false;

    /*******************
     ** Process Frame **
     *******************/
    ++_stats.frames_in;
    bool is_usable = frame->is_command(stomp::StompFrame::commandMessage)
                     && frame->is_header("message-id");
    if (!is_usable) {
      frame->release();
      return true;
    } // if
    ++_stats.frames_in;

   TLOG(LogDebug, << "received message; "
                  << frame->body()
                  << std::endl);

    process_message( frame->body() );

    std::string message_id = frame->get_header("message-id");
    _stomp->ack(message_id, "1");

    frame->release();
    return true;
  } // Worker::run

  bool Worker::process_message(const std::string &body) {
    openframe::Vars *v = new openframe::Vars(body);

    // if this isn't a message we can ack then we don't
    // care
    bool ok = v->is("sr,to,ms,pa");
    if (!ok) {
      delete v;
      return false;
    } // if

    process_message_t pm;
    pm.source = openframe::StringTool::toUpper( v->get("sr") );
    pm.target = openframe::StringTool::toUpper( v->get("to") );
    pm.body = v->get("ms");
    pm.path = v->get("pa");
    pm.id = v->get("id");
    pm.ack = v->get("ack");
    pm.reply_id = v->get("rpl");
    pm.is_ackonly = v->is("ao");
    pm.v = v;

    event_message_to_apns(pm);

    delete v;
    return true;
  } // Worker::process_message

  bool Worker::event_message_to_apns(process_message_t &pm) {
    // was ack only no reply
    if (pm.is_ackonly) return false;

    openframe::Stopwatch sw;
    sw.Start();

   TLOG(LogDebug, << "searching for target "
                  << pm.target
                  << std::endl);

    apns_registers_t res;
    // search for user in apns register
    bool ok = _store->getApnsRegisterByCallsign(pm.target, res);
    double avg = _apns->profile.average("apns.push", sw.Time());
    if (!ok) return false;

    size_t num_sent = 0;
    while( !res.empty() ) {
      apns_register_t *ar = res.front();

      // apns_register.id, apns_register.device_token, apns_register.environment
      TLOG(LogDebug, << "found id "
                     << ar->id
                     << ", device token "
                     << ar->device_token
                     << ", environment "
                     << ar->environment
                     << ", for "
                     << pm.target
                     << std::endl);

      std::stringstream s;
      s << pm.source << ": " << pm.body;
      ok = _store->setApnsPush(ar->id, s.str());
      if (!ok) {
        TLOG(LogError, << "Unable to insert APNS push record: "
                       << s.str()
                       << std::endl);
      } // if

      apns::ApnsMessage *aMessage;
      try {
        aMessage = new apns::ApnsMessage(ar->device_token);
      } // try
      catch (apns::ApnsMessage_Exception e) {
        TLOG(LogWarn, << "Failed to create new APNS message "
                      << pm.target
                      << ">"
                      << pm.source
                      << "; "
                      << e.message()
                      << std::endl);
        delete ar;
        res.pop_front();
        continue;
      } // catch

      aMessage->text(s.str());
      aMessage->actionKeyCaption("View");
      aMessage->badgeNumber(1);

      if (!strcasecmp(ar->environment.c_str(), "prod"))
        aMessage->environment(apns::ApnsMessage::APNS_ENVIRONMENT_PROD);

      _apns->push(aMessage);

      TLOG(LogNotice, << "Queuing APNS to "
                      << pm.target
                      << ": <"
                      << pm.source
                      << "> "
                      << pm.body
                      << std::endl);
      delete ar;
      res.pop_front();

      ++num_sent;
    } // while

    TLOG(LogNotice, << "Queued "
                    << num_sent
                    << " rows to APNS in ("
                    << std::fixed << std::setprecision(4)
                    << sw.Time()
                    << " seconds, "
                    << std::fixed << std::setprecision(4)
                    << avg
                    << " 5min)"
                    << std::endl);

    return true;
  } // Worker::event_message_to_apns

} // namespace aprscreate
