#include <memory>

#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/wifi-net-device.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot-helper.h"
#include "ns3/file-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/packet-socket-helper.h"

#include "tcp-generator.h"

using namespace ns3;

using std::shared_ptr;
using std::make_shared;

NS_LOG_COMPONENT_DEFINE ("AdHocBetweenSwithes");

// Topology:
//             wifi 
//         n1        |             n6
//         |         \/            |
//    n2---n0---n4*          *n9---n5---n7
//         |         ad-hoc        |
//         n3                      n8
//   
//        LAN1                    LAN2
//     10.10.1.0               10.10.2.0
//
// Unit-1-1 = n1    Unit-2-1 = n6
// Unit-1-2 = n2    Unit-2-2 = n7
// Unit-1-3 = n3    Unit-2-3 = n8
// Switch-1 = n0    Switch-2 = n5
// Radio-1  = n4    Radio-2  = n9

static void
CwndChange (std::shared_ptr<Gnuplot2dDataset> dataSet, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_DEBUG (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  dataSet->Add (Simulator::Now ().GetSeconds (), newCwnd);
}

uint64_t g_wifiTxBytes = 0;
void CalculateTxBytes (Ptr<const Packet> packet)
{
  g_wifiTxBytes += packet->GetSize ();
}

void CalculateWifiThroughput (shared_ptr<Gnuplot2dDataset> dataSet, uint64_t lastBytes)
{
  Time now = Simulator::Now ();                                        
  double cur = (g_wifiTxBytes - lastBytes) * (double) 8 / 1e5;   
  NS_LOG_DEBUG (now.GetSeconds () << '\t' << cur);
  dataSet->Add (now.GetSeconds (), cur);
  lastBytes = g_wifiTxBytes;
  Simulator::Schedule (MilliSeconds (100), &CalculateWifiThroughput, dataSet, lastBytes);
}

void PlotDataSet (
  const std::string &plotName, 
  const std::string &title, 
  const std::string &xLenend,
  const std::string &yLegend,
  shared_ptr<Gnuplot2dDataset> dataSet)
{
  const std::string plotFileName = plotName + ".plt";
  const std::string pngFileName = plotName + ".png";

  Gnuplot plot (pngFileName, title);
  plot.SetTerminal ("png");
  plot.SetLegend (xLenend, yLegend);
  plot.AddDataset (*dataSet);

  std::ofstream plotFile (plotFileName);
  plot.GenerateOutput (plotFile);
  
  plotFile.close ();
}

int main (int argc, char *argv[])
{
  std::string wifiMode = "OfdmRate54Mbps";
  uint32_t simulationTime = 120;
  Time csmaDelay ("0ms");

  CommandLine cmd;
  cmd.AddValue ("time", "Simulation time", simulationTime);
  cmd.AddValue ("wifiMode", "Mode of wifi", wifiMode);
  cmd.AddValue ("csmaDelay", "Delay of CSMA channel", csmaDelay);
  cmd.Parse (argc, argv);


  // Создание топологии модели
  NS_LOG_INFO ("Creating topology");
  Ptr<Node> lan1Switch = CreateObject<Node> ();
  NodeContainer lan1Units (3);
  Ptr<Node> lan1WifiNode = CreateObject<Node> ();

  Ptr<Node> lan2Switch = CreateObject<Node> ();
  NodeContainer lan2Units (3);
  Ptr<Node> lan2WifiNode = CreateObject<Node> ();

  // Все узлы локальной сети кроме коммутатора
  NodeContainer lan1CsmaNodes (lan1Units, lan1WifiNode);
  NodeContainer lan2CsmaNodes (lan2Units, lan2WifiNode);

  NodeContainer wifiNodes (lan1WifiNode, lan2WifiNode);

  // Создание Ehternet соединений
  NS_LOG_INFO ("Creating CSMA connections");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("1000Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (csmaDelay)); 

  NetDeviceContainer lan1SwitchDevices;
  NetDeviceContainer lan1CsmaDevices;
  NetDeviceContainer lan2SwitchDevices;
  NetDeviceContainer lan2CsmaDevices;
  
  // Создание соединений для коммутаторов
  for (int i = 0; i < 4; i++) {
    NetDeviceContainer link = csma.Install ( 
      NodeContainer (lan1Switch, lan1CsmaNodes.Get (i))
    );
    lan1SwitchDevices.Add (link.Get (0));
    lan1CsmaDevices.Add (link.Get (1));

    link = csma.Install (NodeContainer (lan2Switch, lan2CsmaNodes.Get (i)));
    lan2SwitchDevices.Add (link.Get (0));
    lan2CsmaDevices.Add (link.Get (1));
  }

  // Создание коммутатора на основе моста (IEEE 802.1D bridging)
  NS_LOG_INFO ("Creating bridges (swithes)");
  BridgeHelper bridge;
  bridge.Install (lan1Switch, lan1SwitchDevices);
  bridge.Install (lan2Switch, lan2SwitchDevices);

  // Настройка wifi 
  NS_LOG_INFO ("Creating wifi connection");
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  WifiMacHelper wifiMac;
  // Использование прозрачного wifi без точек доступа
  wifiMac.SetType ("ns3::AdhocWifiMac");
  
  WifiHelper wifi;
  // Установка пропускной способности
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue (wifiMode),
                                "ControlMode", StringValue (wifiMode));

  NetDeviceContainer wifiDevices = wifi.Install (wifiPhy, wifiMac, wifiNodes);
  
  // Установка расположения wifi-устройств
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (wifiNodes);
  //==============================================

  NS_LOG_INFO ("Setup stack of internet protocols");
  InternetStackHelper stack;
  stack.Install (lan1CsmaNodes);
  stack.Install (lan2CsmaNodes);

  // Установка адресации
  NS_LOG_INFO ("Configuring IP addreses");
  const std::string mask = "255.255.255.0";
  Ipv4AddressHelper address;
  address.SetBase ("10.10.1.0", mask.c_str ());
  Ipv4InterfaceContainer lan1Interfaces = address.Assign (lan1CsmaDevices);

  address.SetBase ("10.10.2.0", mask.c_str ());
  Ipv4InterfaceContainer lan2Interfaces = address.Assign (lan2CsmaDevices);

  address.SetBase ("10.10.3.0", mask.c_str ());
  Ipv4InterfaceContainer wifiInterfaces = address.Assign (wifiDevices);

  // Установка мультикаси маршрутизации
  NS_LOG_INFO ("Setup multicast routing");
  // Адресс мультикаст группы, который будет использоваться при рассылке
  const Ipv4Address multicastGroup = "225.1.2.2";
  const Ipv4Address multicastSource = Ipv4Address::GetAny ();//lan1Interfaces.GetAddress (1);

  Ipv4StaticRoutingHelper routing;
  // Маршрутизация по умолчанию для node-а - отправителя
  Ptr<Node> sender = lan1CsmaNodes.Get (1);
  Ptr<NetDevice> senderDevice = lan1CsmaDevices.Get (1);
  routing.SetDefaultMulticastRoute (sender, senderDevice);

  // Настройка маршрутизации на wifi роутерах
  Ptr<NetDevice> inputDevice = lan1CsmaDevices.Get (3);
  Ptr<NetDevice>  outputDevice = wifiDevices.Get (0);
  routing.AddMulticastRoute (
    lan1WifiNode, multicastSource,  multicastGroup, inputDevice, outputDevice
  );

  inputDevice = wifiDevices.Get (1);;
  outputDevice = lan2CsmaDevices.Get (3);
  routing.AddMulticastRoute (
    lan2WifiNode, multicastSource,  multicastGroup, inputDevice, outputDevice
  );
  // ============================================

  NS_LOG_INFO ("Setup traffic generators");

  const int udpPort = 1, multicastPort = 2, tcpPort = 3;
  const Address udpAnyAddress = InetSocketAddress (Ipv4Address::GetAny (), udpPort);
  const Address tcpAnyAddress = InetSocketAddress (Ipv4Address::GetAny (), tcpPort);
  const Address udpAnyMulticast = InetSocketAddress (Ipv4Address::GetAny (), multicastPort);

  const Time sinkersStartTime (Seconds (1));
  const Time generatorsStartTime (Seconds (2));

  // Установка получателей пакетов для unit1-1 и unit2-1
  PacketSinkHelper udpPacketSinker ("ns3::UdpSocketFactory", udpAnyAddress);
  ApplicationContainer udpSinkers = udpPacketSinker.Install (lan1CsmaNodes.Get (0));
  udpSinkers.Add (udpPacketSinker.Install (lan2CsmaNodes.Get (0)));

  // Установка получателей мультикаст рассылки
  PacketSinkHelper udpMulticastSinker ("ns3::UdpSocketFactory", udpAnyMulticast);
  ApplicationContainer multicastSinkers = udpMulticastSinker.Install (lan1Units);
  multicastSinkers.Add (udpMulticastSinker.Install (lan2Units));

  // Установка получателя для tcp на unit1-3
  PacketSinkHelper tcpPacketSinker ("ns3::TcpSocketFactory", tcpAnyAddress);
  ApplicationContainer sinkers = tcpPacketSinker.Install (lan1CsmaNodes.Get (2));

  // Сбор всех получателей в единый контейнер для удобства
  sinkers.Add (udpSinkers);
  sinkers.Add (multicastSinkers);

  // Генерация трафика 5Mb/s от unut1-1 к unit2-1
  const Address unit21Address = InetSocketAddress (lan2Interfaces.GetAddress (0), udpPort);
  OnOffHelper onOffGenerator ("ns3::UdpSocketFactory", unit21Address);
  onOffGenerator.SetConstantRate (DataRate ("5Mbps"));
  ApplicationContainer generators = onOffGenerator.Install (lan1CsmaNodes.Get (0));

  // Генерация трафика 1Mb/s мультикаст рассылки от unit1-2
  const Address multicastAddress = InetSocketAddress (multicastGroup, multicastPort);
  onOffGenerator.SetConstantRate (DataRate ("1Mbps"));
  onOffGenerator.SetAttribute ("Remote", AddressValue (multicastAddress));
  generators.Add (onOffGenerator.Install (lan1CsmaNodes.Get (1)));

  // Генерация переодического трафика от unit2-1 к unit1-1
  const Address unit11Adderss = InetSocketAddress (lan1Interfaces.GetAddress (0), udpPort);
  onOffGenerator.SetAttribute ("Remote", AddressValue (unit11Adderss));
  onOffGenerator.SetAttribute (
    "OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]")
  );
  onOffGenerator.SetAttribute (
    "OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=40.0]")
  );
  onOffGenerator.SetAttribute ("DataRate", DataRateValue (DataRate ("20Mbps")));
  generators.Add (onOffGenerator.Install (lan2CsmaNodes.Get (0)));

  // Генерация TCP трафика от unit2-3 к unit1-3 
  const Address unit13Address = InetSocketAddress (lan1Interfaces.GetAddress (2), tcpPort);
  // Создание сокета отдельно от приложения для того, чтобы была возможность
  // до начала симуляции закрепить TraceSource
  Ptr<Socket> tcpSocket = Socket::CreateSocket (lan2CsmaNodes.Get (2), 
                                                TcpSocketFactory::GetTypeId ());

  Ptr<TcpGeneratorApp> tcpGenerator = CreateObject<TcpGeneratorApp> (
    tcpSocket, unit13Address, DataRate ("40Mbps")
  );
  lan2CsmaNodes.Get (2)->AddApplication (tcpGenerator);
  generators.Add (tcpGenerator);

  sinkers.Start (sinkersStartTime);
  generators.Start (generatorsStartTime);
  // ============================================

  // Сбор данных о симуляции
  // Дата сеты для построения графиков
  auto cwndDataSet = make_shared<Gnuplot2dDataset> ("cwnd");
  auto wifiTxThroughputSet = make_shared<Gnuplot2dDataset> ("tx");

  // Сбор Congastion window для анализа окна перегрузки
  tcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, cwndDataSet));

  // Подсчет пропускной способности wifi канала
  Config::ConnectWithoutContext (
    "/NodeList/9/DeviceList/1/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/PhyTxEnd",
    MakeCallback (&CalculateTxBytes)
  );
  Simulator::Schedule (Seconds (0), &CalculateWifiThroughput, wifiTxThroughputSet, 0);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();  

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  // Построение полученных данных
  PlotDataSet (
    "cwnd", 
    "Congestion window",
    "Time (Seconds)", 
    "Congestion window size (cwnd)",
    cwndDataSet
  );

  PlotDataSet (
    "wifi-th", 
    "Wifi Throughput", 
    "Time (Seconds)", 
    "Data Rate (Mb/s)", 
    wifiTxThroughputSet
  );

  Simulator::Destroy ();
  return 0;
}
