#ifndef APRSINJECT_APP_H
#define APRSINJECT_APP_H

#include <string>
#include <deque>

#include <openframe/openframe.h>
#include <openframe/App/Server.h>
#include <stomp/StompStats.h>

namespace apnspusher {
/**************************************************************************
 ** General Defines                                                      **
 **************************************************************************/
#define DEVEL_CA_CERT_PATH        CFG_STRING("module.apns.devel.capath", "Certs")

#define DEVEL_RSA_CLIENT_CERT     CFG_STRING("module.apns.devel.cert", "Certs/apn-dev-cert.pem")
#define DEVEL_RSA_CLIENT_KEY      CFG_STRING("module.apns.devel.key", "Certs/apn-dev-key.pem")
// Development Connection Infos
#define DEVEL_APPLE_HOST          CFG_STRING("module.apns.devel.push.host", "gateway.sandbox.push.apple.com")
#define DEVEL_APPLE_PORT          CFG_INT("module.apns.devel.push.port", 2195)
#define DEVEL_APPLE_TIMEOUT       CFG_INT("module.apns.devel.push.timeout", 60)
#define DEVEL_APPLE_FEEDBACK_HOST CFG_STRING("module.apns.devel.feedback.host", "feedback.sandbox.push.apple.com")
#define DEVEL_APPLE_FEEDBACK_PORT CFG_INT("module.apns.devel.feedback.port", 2196)
#define DEVEL_APPLE_FEEDBACK_TIMEOUT CFG_INT("module.apns.devel.feedback.timeout", 60)

// Production Certificates
#define PROD_CA_CERT_PATH        CFG_STRING("module.apns.prod.capath", "Certs")
#define PROD_RSA_CLIENT_CERT     CFG_STRING("module.apns.prod.cert", "Certs/apn-prod-cert.pem")
#define PROD_RSA_CLIENT_KEY      CFG_STRING("module.apns.prod.key", "Certs/apn-prod-key.pem")

// Production Connection Infos
#define PROD_APPLE_HOST          CFG_STRING("module.apns.prod.push.host", "gateway.push.apple.com")
#define PROD_APPLE_PORT          CFG_INT("module.apns.prod.push.port", 2195)
#define PROD_APPLE_TIMEOUT       CFG_INT("module.apns.prod.push.timeout", 60)
#define PROD_APPLE_FEEDBACK_HOST CFG_STRING("module.apns.prod.feedback.host", "feedback.push.apple.com")
#define PROD_APPLE_FEEDBACK_PORT CFG_INT("module.apns.prod.feedback.port", 2196)
#define PROD_APPLE_FEEDBACK_TIMEOUT CFG_INT("module.apns.prod.feedback.timeout", 86400)


/**************************************************************************
 ** Structures                                                           **
 **************************************************************************/
  class App : public openframe::App::Server {
    public:
      typedef openframe::App::Server super;

      typedef std::deque<pthread_t> workers_t;
      typedef workers_t::iterator workers_itr;
      typedef workers_t::const_iterator workers_citr;
      typedef workers_t::size_type workers_st;

      static const char *kPidFile;

      App(const std::string &prompt, const std::string &config, const bool console=false);
      virtual ~App();

      void onInitializeSystem();
      void onInitializeConfig();
      void onInitializeCommands();
      void onInitializeDatabase();
      void onInitializeModules();
      void onInitializeThreads();

      void onDeinitializeSystem();
      void onDeinitializeCommands();
      void onDeinitializeDatabase();
      void onDeinitializeModules();
      void onDeinitializeThreads();

      void rcvSighup();
      void rcvSigusr1();
      void rcvSigusr2();
      void rcvSigint();
      void rcvSigpipe();

      bool onRun();

      static void *WorkerThread(void *arg);

      stomp::StompStats *stats() { return _stats; }

    protected:
    private:
      workers_t _workers;
      stomp::StompStats *_stats;
  }; // App

/**************************************************************************
 ** Macro's                                                              **
 **************************************************************************/

/**************************************************************************
 ** Proto types                                                          **
 **************************************************************************/
} // namespace apnspusher

extern apnspusher::App *app;
extern openframe::Logger elog;
#endif
