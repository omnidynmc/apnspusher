#include <cassert>
#include <exception>
#include <iostream>
#include <new>
#include <string>

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <openframe/openframe.h>
#include <stomp/Stomp.h>
#include <stomp/StompFrame.h>
#include <stomp/StompClient.h>
#include <stomp/StompHeader.h>

int main(int argc, char **argv) {

  if (argc < 3) {
    std::cerr << argv[0] << " <stomp host> <to>" << std::endl;
    return 1;
  } // if

  std::string hosts = argv[1];
  std::string to = argv[2];

  stomp::Stomp *stomp;
  try {
    stomp = new stomp::Stomp(hosts, "user", "pass");
  } // try
  catch(stomp::Stomp_Exception ex) {
    std::cout << "ERROR: " << ex.message() << std::endl;
    return 0;
  } // catch

  std::cout << "Connecting to stomp server" << std::endl;

  std::cout << "Sending message" << std::endl;
  openframe::Vars v;
  // CT:1329675095|ID:38|MS:ESE TIENE TONO 100 EL DE LA ESTRADA|PA:APU25N,EA1URF-3,ED1YAX-3,EA1RCI-3*,WIDE3,qAR,EB1FJK-10|RPL:38|SR:EA1EOL-10|TO:EA1CC-9
  v.add("ct", openframe::stringify<int>( time(NULL) ) );
  v.add("id", "1");
  v.add("ms", "This is a test message");
  v.add("pa", "APU25N,EA1URF-3,ED1YAX-3,EA1RCI-3*,WIDE3,qAR,EB1FJK-10");
  v.add("rpl", "38");
  v.add("sr", "N6NAR-1");
  v.add("to", to);
  stomp->send("/topic/notify.aprs.messages", v.compile() );

  std::cout << "Disconnecting" << std::endl;

  return 0;
} // main
