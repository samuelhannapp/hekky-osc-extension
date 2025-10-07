#include "hekky-osc.hpp"

namespace hekky {
    namespace osc {
        uint64_t UdpSender::m_openSockets = 0;

        const bool UdpSender::IsAlive() {
            return m_isAlive;
        }

        UdpSender::UdpSender() : m_address(""), m_portOut(0), m_portIn(0), m_isAlive(false)
#ifdef HEKKYOSC_WINDOWS
            , m_destinationAddress({ 0 }), m_localAddress({ 0 }), m_nativeSocket(INVALID_SOCKET)
#endif
        {
        }

        UdpSender::UdpSender(const std::string& ipAddress, uint32_t portOut, uint32_t portIn, network::OSC_NetworkProtocol protocol)
            : m_address(ipAddress), m_portOut(portOut), m_portIn(portIn)
#ifdef HEKKYOSC_WINDOWS
            , m_destinationAddress({ 0 }), m_localAddress({ 0 }), m_nativeSocket(INVALID_SOCKET)
#endif
#if defined(HEKKYOSC_LINUX) || defined(HEKKYOSC_MAC)
            , m_destinationAddress({ 0 }), m_localAddress({ 0 }), m_nativeSocket(0)
#endif
        {
            m_isAlive = false;
#ifdef HEKKYOSC_WINDOWS
            int result = 0;
            if (m_openSockets < 1) {
                WSADATA wsaData;
                // @TODO: Skip if other connections are open
                result = WSAStartup(MAKEWORD(2, 2), &wsaData);
                if (result != 0) {
                    HEKKYOSC_ASSERT(result == 0, "WSAStartup failed");
                    return;
                }

                m_openSockets++;
            }

            // Get localhost as a native network address
            m_localAddress.sin_family = AF_INET;
            m_localAddress.sin_addr.s_addr = INADDR_ANY;
            //             result = inet_pton(AF_INET, "127.0.0.1", &m_localAddress.sin_addr.s_addr);
            //             if (result == 0) {
            //                 HEKKYOSC_ASSERT(result == 0, "Invalid IP Address!");
            //                 return;
            //             } else if (result == -1) {
            // #ifdef HEKKYOSC_DOASSERTS
            //                 int errorCode = WSAGetLastError();
            //                 HEKKYOSC_ERR(std::string("WSA Error code: ") + std::to_string(errorCode) + "\nFor more information, please visit https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-inet_pton#return-value.\n");
            // #endif
            //                 HEKKYOSC_ASSERT(result == -1, "Failed to set IP Address!");
            //                 return;
            //             }
            m_localAddress.sin_port = htons(m_portIn);

            // Get the destination as a native network address
            m_destinationAddress.sin_family = AF_INET;
            result = inet_pton(AF_INET, m_address.c_str(), &m_destinationAddress.sin_addr.s_addr);
            if (result == 0) {
                HEKKYOSC_ASSERT(result == 0, "Invalid IP Address!");
                return;
            }
            else if (result == -1) {
#ifdef HEKKYOSC_DOASSERTS
                int errorCode = WSAGetLastError();
                HEKKYOSC_ERR(std::string("WSA Error code: ") + std::to_string(errorCode) + "\nFor more information, please visit https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-inet_pton#return-value.\n");
#endif
                HEKKYOSC_ASSERT(result == -1, "Failed to set IP Address!");
                return;
            }
            m_destinationAddress.sin_port = htons(m_portOut);

            // Open the network socket
            m_nativeSocket = socket(AF_INET, SOCK_DGRAM, protocol == network::OSC_NetworkProtocol::UDP ? IPPROTO_UDP : IPPROTO_TCP);
            if (m_nativeSocket == INVALID_SOCKET) {
#ifdef HEKKYOSC_DOASSERTS
                int errorCode = WSAGetLastError();
                HEKKYOSC_ERR(std::string("WSA Error code: ") + std::to_string(errorCode) + "\nFor more information, please visit https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-socket#return-value.\n");
#endif
                HEKKYOSC_ASSERT(result == SOCKET_ERROR, "Failed to create network socket!");
                return;
            }
            result = bind(m_nativeSocket, (sockaddr*)&m_localAddress, sizeof(m_localAddress));
            if (result == SOCKET_ERROR) {
#ifdef HEKKYOSC_DOASSERTS
                int errorCode = WSAGetLastError();
                HEKKYOSC_ERR(std::string("WSA Error code: ") + std::to_string(errorCode) + "\nFor more information, please visit https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-bind#return-value.\n");
#endif
                HEKKYOSC_ASSERT(result == SOCKET_ERROR, "Failed to bind to network socket!");
                return;
            }

            // If we reached this point, we have successfully initialized a network socket!
            m_isAlive = true;
#endif

#if defined(HEKKYOSC_LINUX) || defined(HEKKYOSC_MAC)
            struct hostent* h;
            //check ip adress
            h = gethostbyname(m_address.c_str());
            if (h == NULL) {
                HEKKYOSC_ASSERT(result == 0, "Invalid IP Address!");
                return;
                //exit (EXIT_FAILURE);
            }
            m_openSockets++;
            m_destinationAddress.sin_family = h->h_addrtype;
            memcpy((char*)&m_destinationAddress.sin_addr.s_addr, h->h_addr_list[0], h->h_length);
            m_destinationAddress.sin_port = htons(m_portOut);
            // Open the network socket
            m_nativeSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (m_nativeSocket < 0) {
                HEKKYOSC_ASSERT(result == SOCKET_ERROR, "Cannot open Socket!");
                return;
                //exit (EXIT_FAILURE);
            }
            //Bind network socket
            m_localAddress.sin_family = AF_INET;
            m_localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
            m_localAddress.sin_port = htons(m_portIn);
            result = bind(m_nativeSocket, (struct sockaddr*)&m_localAddress, sizeof(m_localAddress));
            if (result < 0) {
                HEKKYOSC_ASSERT(result == SOCKET_ERROR, "Failed to bind to network socket!");
                return;
                //exit (EXIT_FAILURE);
            }
            m_isAlive = true;
#endif
        }

        UdpSender::~UdpSender() {
            if (m_isAlive) {
                Close();
            }
            else {
                HEKKYOSC_ASSERT(false, "Tried destroying OSC Server, but the server is not running!");
            }
        }

        void UdpSender::Close() {
#ifdef HEKKYOSC_WINDOWS
            HEKKYOSC_ASSERT(m_nativeSocket != INVALID_SOCKET, "Tried destorying native socket, but the native socket is null! Has the socket already been destroyed?");
            HEKKYOSC_ASSERT(m_isAlive == true, "Tried closing OSC Server, but the OSC Server is not running! Has the OSC Server already been destroyed?");

            closesocket(m_nativeSocket);

            m_isAlive = false;
            m_openSockets--;

            if (m_openSockets < 1) {
                // @TODO: Destroy if total connections == 0, use a static variable to keep track
                WSACleanup();
            }
#endif
        }

        void UdpSender::Send(char* data, int size) {
#ifdef HEKKYOSC_WINDOWS
            HEKKYOSC_ASSERT(m_nativeSocket != INVALID_SOCKET, "Tried sending a packet, but the native socket is null! Has the socket been initialized?");
            HEKKYOSC_ASSERT(m_isAlive == true, "Tried sending a packet, but the server isn't running!");

            // Skip if the data is 0 or somehow negative (should be theoretically impossible since this is an unsigned integer)
            if (size < 1)
                return;

            // Send data over the socket
            sendto(m_nativeSocket, data, size, 0, (sockaddr*)&m_destinationAddress, sizeof(m_destinationAddress));
#endif
#if defined(HEKKYOSC_LINUX) || defined(HEKKYOSC_MAC)
            HEKKYOSC_ASSERT(m_nativeSocket != 0, "Tried sending a packet, but the native socket is null! Has the socket been initialized?");
            HEKKYOSC_ASSERT(m_isAlive == true, "Tried sending a packet, but the server isn't running!");

            // Skip if the data is 0 or somehow negative (should be theoretically impossible since this is an unsigned integer)
            if (size < 1)
                return;
            // Send data over the socket
            sendto(m_nativeSocket, data, size, 0, (struct sockaddr*)&m_destinationAddress, sizeof(m_destinationAddress)); //thats exactly the same, we should get rid of that one
#endif
#ifdef HEKKYOSC_STM32
            udpClient_send(data, size);
#endif
        }

        void UdpSender::Send(OscPacket& packet) {
#ifdef HEKKYOSC_WINDOWS
            HEKKYOSC_ASSERT(m_nativeSocket != INVALID_SOCKET, "Tried sending a packet, but the native socket is null! Has the socket been initialized?");
            HEKKYOSC_ASSERT(m_isAlive == true, "Tried sending a packet, but the server isn't running!");

            int size = 0;
            char* data = packet.GetBytes(size);

            // Send data over the socket
            Send(data, size);
#endif
#if defined(HEKKYOSC_LINUX) || defined(HEKKYOSC_MAC) || defined(HEKKYOSC_STM32)
            HEKKYOSC_ASSERT(m_nativeSocket != 0, "Tried sending a packet, but the native socket is null! Has the socket been initialized?");
            HEKKYOSC_ASSERT(m_isAlive == true, "Tried sending a packet, but the server isn't running!");

            int size = 0;
            char* data = packet.GetBytes(size);

            // Send data over the socket
            Send(data, size);
#endif
        }
        hekky::osc::OscMessage  UdpSender::Receive() {
#ifdef HEKKYOSC_WINDOWS
            char buffer[1024];
            int buffer_length = 1024;
            int res = 0;
            struct sockaddr_in sender_address;
            int sender_address_size = sizeof(sender_address);
            res = recvfrom(m_nativeSocket, buffer, buffer_length, 0, (SOCKADDR*)&sender_address, &sender_address_size);
            auto message = hekky::osc::OscMessage(buffer, buffer_length);
            return message;
#endif
#if defined(HEKKYOSC_LINUX) || defined(HEKKYOSC_MAC)
            char buffer[1024];
            int buffer_length = 1024;
            int res = 0;
            struct sockaddr_in sender_address;
            int sender_address_size = sizeof(sender_address);
            res = recvfrom(m_nativeSocket, buffer, buffer_length, 0, (struct sockaddr*)&sender_address, (socklen_t*)&sender_address_size);
            auto message = hekky::osc::OscMessage(buffer, buffer_length);
            return message;
#endif

        }
    }
}