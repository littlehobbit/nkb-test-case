#include "tcp-generator.h"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"

TcpGeneratorApp::TcpGeneratorApp (
    Ptr<Socket> socket, 
    Address address, 
    DataRate dataRate, 
    uint32_t packetSize
  )
  : m_socket (socket), 
    m_peer (address), 
    m_packetSize (packetSize),  
    m_dataRate (dataRate), 
    m_sendEvent (), 
    m_running (false)
{
}

TcpGeneratorApp::~TcpGeneratorApp()
{
  m_socket = nullptr;
}

void
TcpGeneratorApp::Setup (
    Ptr<Socket> socket, 
    Address address, 
    DataRate dataRate, 
    uint32_t packetSize
  )
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_dataRate = dataRate;
}

void
TcpGeneratorApp::StartApplication (void)
{
  m_running = true;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}

void 
TcpGeneratorApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void 
TcpGeneratorApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);
  ScheduleTx ();
}

void 
TcpGeneratorApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &TcpGeneratorApp::SendPacket, this);
    }
}
