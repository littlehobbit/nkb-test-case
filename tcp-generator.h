#ifndef TCP_GENERATOR_H
#define TCP_GENERATOR_H

// #include "ns3/core-module.h"
#include "ns3/applications-module.h"
// #include "ns3/internet-module.h"

using namespace ns3;

class TcpGeneratorApp : public Application 
{
public:

  TcpGeneratorApp (Ptr<Socket> socket, Address address, DataRate dataRate, uint32_t packetSize = 512);
  virtual ~TcpGeneratorApp ();

  void Setup (Ptr<Socket> socket, Address address, DataRate dataRate, uint32_t packetSize = 512);

private:
  void StartApplication (void) override;
  void StopApplication (void) override;

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
};

#endif