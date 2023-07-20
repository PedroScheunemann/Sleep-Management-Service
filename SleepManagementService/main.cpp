#include "globals.cpp"

// Variáveis do manager
pthread_mutex_t mutex_table = PTHREAD_MUTEX_INITIALIZER;
guestTable guests;

// Variáveis do participante
pthread_mutex_t mutex_guest_out = PTHREAD_MUTEX_INITIALIZER;
int current_guest_is_out = 0;

// Funções do participante
static void guest_program();
void* guest_discovery_service(void *arg);
void* guest_monitoring_service(void *arg);
void* guest_interface_service(void *arg);
string get_host_name();
string get_mac_address();
string get_ip_address();
string pack_discovery_sending_message();

// Funções do manager
static void manager_program();
void* manager_discovery_service(void *arg);
void* manager_monitoring_service(void *arg);
void* manager_interface_service(void *arg);
guest parse_payload(char* payload);
void send_wake_on_lan_packet(string mac_address);

int main(int argc, char **argv)
{
    //./sleep_server
    if (argc == 1)
    {
        guest_program();
    }
    // ./sleep_server manager
    else if (argc == 2 && strcmp(argv[1], "manager") == 0)
    {
        manager_program();
    }
}

static void guest_program()
{
    pthread_t thread_discovery, thread_monitoring, thread_interface;

    // criando as threads
    if (pthread_create(&thread_discovery, nullptr, guest_discovery_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_create(&thread_monitoring, nullptr, guest_monitoring_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação da thread de monitoramento." << endl;
        exit(1);
    }
    if (pthread_create(&thread_interface, nullptr, guest_interface_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação da thread da interface." << endl;
        exit(1);
    }

    // thread pai espera elas terminarem
    pthread_join(thread_discovery, nullptr);
    pthread_join(thread_monitoring, nullptr);
    pthread_join(thread_interface, nullptr);
}

static void manager_program()
{
    pthread_t thread_discovery, thread_monitoring, thread_interface;

    // criando as threads
    if (pthread_create(&thread_discovery, nullptr, manager_discovery_service , nullptr) != 0)
    {
        cout << "MANAGER: Erro na criação da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_create(&thread_monitoring, nullptr, manager_monitoring_service, nullptr) != 0)
    {
        cout << "MANAGER: Erro na criação da thread de monitoramento." << endl;
        exit(1);
    }
    if (pthread_create(&thread_interface, nullptr, manager_interface_service, nullptr) != 0)
    {
        cout << "MANAGER: Erro na criação da thread da interface." << endl;
        exit(1);
    }

    // thread pai espera elas terminarem
    pthread_join(thread_discovery, nullptr);
    pthread_join(thread_monitoring, nullptr);
    pthread_join(thread_interface, nullptr);
}

// Envia pacotes do tipo SLEEP_SERVICE_DISCOVERY para as estações por broadcast e espera ser respondido pelo manager.
// CLIENTE
void* guest_discovery_service(void *arg)
{
    // Criando o descritor do socket do participante
    int guest_socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (guest_socket_descriptor < 0)
    {
        cout << "PARTICIPANTE: Erro na criação do socket [DESCOBERTA]." << endl;
        exit(1);
    }

    // Setando a opção de broadcast do socket (habilitando o envio de broadcast messages)
    int enable_broadcast = 1;
    if (setsockopt(guest_socket_descriptor, SOL_SOCKET, SO_BROADCAST, &enable_broadcast, sizeof(int)) < 0)
    {
        cout << "PARTICIPANTE: Erro ao setar o modo broadcast do socket [DESCOBERTA]." << endl;
        close(guest_socket_descriptor);
        exit(1);
    }

    // Setando um timeout
    struct timeval timeout;
    timeout.tv_sec = 4;    // 4 segundos esperando resposta do manager
    timeout.tv_usec = 0;
    if (setsockopt(guest_socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        cout << "PARTICIPANTE: Erro ao setar o timeout do socket [DESCOBERTA]." << endl;
        close(guest_socket_descriptor);
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO ENDERE�OS //////////////////////////////////
    struct sockaddr_in sending_address;     // endereço para onde mandaremos mensagem
    struct sockaddr_in manager_address;     // endereço de onde vai vir a resposta (que será do manager)
    socklen_t address_len = sizeof(struct sockaddr_in);

    sending_address.sin_family = AF_INET;
	sending_address.sin_port = htons(PORT_DISCOVERY_SERVICE);  // envia para a porta PORT_DISCOVERY_SERVICE
	sending_address.sin_addr.s_addr = htonl(INADDR_BROADCAST); // pro endereço de broadcast
    ////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////// CONFIGURANDO MENSAGEM ///////////////////////////////////
    packet* sending_message = new packet();  // mensagem que será enviada para todas as estações (hostname;mac_address;ip_address;)
    packet* manager_message = new packet();  // mensagem de resposta que será recebida do manager
    size_t  message_len = sizeof(packet);

    sending_message->type = SLEEP_SERVICE_DISCOVERY;
    strcpy(sending_message->payload, pack_discovery_sending_message().c_str());
    ////////////////////////////////////////////////////////////////////////////////////////////

    while(1)
    {
        // Enviando mensagem para todo mundo
        if (sendto(guest_socket_descriptor, sending_message, message_len, 0,(struct sockaddr *) &sending_address, address_len) < 0)
        {
            cout << "PARTICIPANTE: Erro ao enviar mensagem de descoberta." << endl;
            close(guest_socket_descriptor);
            exit(1);
        }
        // Esperando resposta do manager chegar até timeout
        if (recvfrom(guest_socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &manager_address, &address_len) >= 0)
        {
            break; // Se chegou, sai do loop, senão, tenta de novo
        }
    }

    close(guest_socket_descriptor);
    return nullptr;
}

// Recebendo e respondendo pacotes do tipo SLEEP_STATUS_REQUEST para o manager.
// SERVIDOR
void* guest_monitoring_service(void *arg)
{
    // Criando o descritor do socket do participante
    int guest_socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (guest_socket_descriptor < 0)
    {
        cout << "PARTICIPANTE: Erro na criação do socket [MONITORAMENTO]." << endl;
        exit(1);
    }

    // Setando a opção REUSEADDR do socket
    int enable_reuseaddr = 1;
    if (setsockopt(guest_socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &enable_reuseaddr, sizeof(int)) < 0)
    {
        cout << "PARTICIPANTE: Erro ao setar o modo REUSEADDR do socket [MONITORAMENTO]." << endl;
        close(guest_socket_descriptor);
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO ENDERE�OS //////////////////////////////////
    struct sockaddr_in guest_address;        // endereço local do guest
    struct sockaddr_in manager_address;      // endereço de onde vem o pacote (que será do manager)
    socklen_t address_len = sizeof(struct sockaddr_in);

    guest_address.sin_family = AF_INET;
	guest_address.sin_port = htons(PORT_MONITORING_SERVICE);  // recebe na porta PORT_MONITORING_SERVICE
	guest_address.sin_addr.s_addr = htonl(INADDR_ANY);
    ////////////////////////////////////////////////////////////////////////////////////////////

    // Endereçando o socket
    if (bind(guest_socket_descriptor, (struct sockaddr*) &guest_address, address_len) < 0)
    {
        cout << "PARTICIPANTE: Erro ao endereçar o socket do servidor de monitoramento." << endl;
        close(guest_socket_descriptor);
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO MENSAGEM ///////////////////////////////////
    packet* manager_message = new packet();    // mensagem que será recebida do manager
    packet* guest_message = new packet();      // mensagem de resposta que será enviada pelo guest
    size_t  message_len = sizeof(packet);

    guest_message->type = SLEEP_STATUS_REQUEST;
    ////////////////////////////////////////////////////////////////////////////////////////////

    while (1)
    {
        // Espera receber mensagem do manager
        if (recvfrom(guest_socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &manager_address, &address_len) < 0)
        {
            cout << "PARTICIPANTE: Erro ao receber mensagem do manager [MONITORAMENTO]." << endl;
            close(guest_socket_descriptor);
            exit(1);
        }
        else
        {
            // Se manager mandou mensagem de monitoramento, responde
            if (manager_message->type == SLEEP_STATUS_REQUEST)
            {
                // Se o participante quer sair do serviço, envia mensagem avisando que quer sair
                if (current_guest_is_out)
                {
                    strcpy(guest_message->payload, STATUS_QUIT);
                }
                // Senão, envia mensagem avisando que está acordado
                else
                {
                    strcpy(guest_message->payload, STATUS_AWAKE);
                }
                if (sendto(guest_socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &manager_address, address_len) < 0)
                {
                    cout << "PARTICIPANTE: Erro ao enviar mensagem ao manager [MONITORAMENTO]." << endl;
                    close(guest_socket_descriptor);
                    exit(1);
                }
            }
        }
    }
    close(guest_socket_descriptor);
    return nullptr;
}

// Fica processando a entrada do usuário pra ver se vai vir um pedido de EXIT.
// Se chegar, notifica que participante quer sair.
void* guest_interface_service(void *arg)
{
    string input;
    cout << "To quit the service type EXIT." << endl;
    cin >> input;
    if (input == "EXIT")
    {
        // Garantindo acesso exclusivo a variável current_guest_is_out
        pthread_mutex_lock(&mutex_guest_out);
        current_guest_is_out = 1;
        pthread_mutex_unlock(&mutex_guest_out);
    }
    return nullptr;
}

// Recebendo e respondendo pacotes do tipo SLEEP_SERVICE_DISCOVERY que chegaram através de broadcast.
// SERVIDOR
void* manager_discovery_service(void *arg)
{
    // Criando o descritor do socket do manager
    int manager_socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (manager_socket_descriptor < 0)
    {
        cout << "MANAGER: Erro na criação do socket [DESCOBERTA]." << endl;
        exit(1);
    }

    // Setando a opção de broadcast do socket (habilitando o receive de broadcast messages)
    int enable_broadcast = 1;
    if (setsockopt(manager_socket_descriptor, SOL_SOCKET, SO_BROADCAST, &enable_broadcast, sizeof(int)) < 0)
    {
        cout << "MANAGER: Erro ao setar o modo broadcast do socket [DESCOBERTA]." << endl;
        close(manager_socket_descriptor);
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO ENDERE�OS //////////////////////////////////
    struct sockaddr_in manager_address;      // endereço local do manager
    struct sockaddr_in guest_address;        // endereço de onde vem o pacote (que será do participante)
    socklen_t address_len = sizeof(struct sockaddr_in);

    manager_address.sin_family = AF_INET;
	manager_address.sin_port = htons(PORT_DISCOVERY_SERVICE);  // recebe na porta PORT_DISCOVERY_SERVICE
	manager_address.sin_addr.s_addr = htonl(INADDR_ANY);
    ////////////////////////////////////////////////////////////////////////////////////////////

    // Endereçando o socket
    if (bind(manager_socket_descriptor, (struct sockaddr*) &manager_address, address_len) < 0)
    {
        cout << "MANAGER: Erro ao endereçar o socket [DESCOBERTA]." << endl;
        close(manager_socket_descriptor);
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO MENSAGEM ///////////////////////////////////
    packet* guest_message = new packet();    // mensagem que será recebida do participante
    packet* manager_message = new packet();  // mensagem de resposta que será enviada pelo manager
    size_t  message_len = sizeof(packet);

    manager_message->type = SLEEP_SERVICE_DISCOVERY;
    strcpy(manager_message->payload, GUEST_DISCOVERED);
    ////////////////////////////////////////////////////////////////////////////////////////////

    while (1)
    {
        // Espera receber mensagem de algum participante
        if (recvfrom(manager_socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &guest_address, &address_len) < 0)
        {
            cout << "MANAGER: Erro ao receber mensagem de participante [DESCOBERTA]." << endl;
            close(manager_socket_descriptor);
            exit(1);
        }
        else
        {
            // Se encontrou um participante, envia resposta de confirmação
            if (guest_message->type == SLEEP_SERVICE_DISCOVERY)
            {
                if (sendto(manager_socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &guest_address, address_len) < 0)
                {
                    cout << "MANAGER: Erro ao enviar mensagem a participante [DESCOBERTA]." << endl;
                    close(manager_socket_descriptor);
                    exit(1);
                }
                // Adiciona na tabela de paticipantes
                guest new_guest = parse_payload(guest_message->payload);
                new_guest.status = "awake";
                // Garantindo acesso exclusivo a tabela
                pthread_mutex_lock(&mutex_table);
                guests.addGuest(new_guest);
                guests.printTable();
                pthread_mutex_unlock(&mutex_table);
            }
        }
    }
    close(manager_socket_descriptor);
    return nullptr;
}

// Enviando pacotes do tipo SLEEP_STATUS_REQUEST para as estações já conhecidas e processando a resposta.
// CLIENTE
void* manager_monitoring_service(void *arg)
{
    // Criando o descritor do socket do manager
    int manager_socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (manager_socket_descriptor < 0)
    {
        cout << "MANAGER: Erro na criação do socket [MONITORAMENTO]." << endl;
        exit(1);
    }

    // Setando a a opção REUSEADDR do socket
    int enable_reuseaddr = 1;
    if (setsockopt(manager_socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &enable_reuseaddr, sizeof(int)) < 0)
    {
        cout << "MANAGER: Erro ao setar o modo REUSEADDR do socket [MONITORAMENTO]." << endl;
        close(manager_socket_descriptor);
        exit(1);
    }

    // Setando um timeout
    struct timeval timeout;
    timeout.tv_sec = 4;    // 4 segundos esperando resposta
    timeout.tv_usec = 0;
    if (setsockopt(manager_socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        cout << "MANAGER: Erro ao setar o timeout do socket [MONITORAMENTO]." << endl;
        close(manager_socket_descriptor);
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO ENDEREÇOS //////////////////////////////////
    struct sockaddr_in guest_address;     // endereço que receberá cada participante para onde mandaremos e receberemos mensagem
    socklen_t address_len = sizeof(struct sockaddr_in);

    guest_address.sin_family = AF_INET;
    ////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////// CONFIGURANDO MENSAGEM ///////////////////////////////////
    packet* manager_message = new packet();    // mensagem que será enviada para os participantes
    packet* guest_message = new packet();      // mensagem de resposta que será recebida dos participantes
    size_t  message_len = sizeof(packet);

    manager_message->type = SLEEP_STATUS_REQUEST;
    strcpy(manager_message->payload, STATUS_REQUEST);
    ////////////////////////////////////////////////////////////////////////////////////////////

    while (1)
    {
        // Pega a lista de IPs dos participantes
        pthread_mutex_lock(&mutex_table);
        vector<string> guests_list_ip_adresses = guests.returnGuestsIpAddressList();
        pthread_mutex_unlock(&mutex_table);

        // Envia mensagens para todos os IPs
        for(vector<string>::iterator it = guests_list_ip_adresses.begin(); it != guests_list_ip_adresses.end(); ++it)
        {
            guest_address.sin_port = htons(PORT_MONITORING_SERVICE);
            inet_aton(it->c_str(), &guest_address.sin_addr);  // envia para (PORT_MONITORING_SERVICE, IP do participante)

            if (sendto(manager_socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &guest_address, address_len) < 0) {
                cout << "MANAGER: Erro ao enviar mensagem para algum participante." << endl;
                close(manager_socket_descriptor);
                exit(1);
            }

            // Esperando resposta chegar
            if (recvfrom(manager_socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &guest_address, &address_len) < 0)
            {
                // Se resposta não chegou
                pthread_mutex_lock(&mutex_table);
                if (guests.getGuestStatus(*it) == "awake")
                {
                    guests.guestSlept(*it); // Atualiza status do participante para "asleep"
                    guests.printTable();    // e printa
                }
                pthread_mutex_unlock(&mutex_table);
            }
            else
            {
                // Se resposta chegou, e foi do tipo SLEEP_STATUS_REQUEST
                if (guest_message->type == SLEEP_STATUS_REQUEST)
                {
                    // Se foi avisando que está acordado
                    if (strcmp(guest_message->payload, STATUS_AWAKE) == 0)
                    {
                        pthread_mutex_lock(&mutex_table);
                        if (guests.getGuestStatus(*it) == "asleep")
                        {
                            guests.wakeGuest(*it); // Atualiza status do participante para "awake"
                            guests.printTable();   // e printa
                        }
                        pthread_mutex_unlock(&mutex_table);
                    }
                    // Se foi avisando que está saindo
                    if (strcmp(guest_message->payload, STATUS_QUIT) == 0)
                    {
                        pthread_mutex_lock(&mutex_table);
                        guests.deleteGuest(*it);   // Remove participante da tabela
                        guests.printTable();       // e printa
                        pthread_mutex_unlock(&mutex_table);
                    }
                }
            }
        }
    }
    close(manager_socket_descriptor);
    return nullptr;
}

// Fica processando a entrada do usuário pra ver se vai vir um pedido de WAKEUP <hostname>.
// Se chegar, manda um comando wakeonlan para seu endereço MAC.
void* manager_interface_service(void *arg)
{
    string WAKEUP, hostname;
    while(1)
    {
        cout << "To wake a guest type WAKEUP <hostname>. " << endl;
        cin >> WAKEUP >> hostname;
        if (WAKEUP == "WAKEUP")
        {
            string mac_address = guests.returnGuestMacAddress(hostname);
            if (mac_address.empty())
            {
                cout << "Hostname not found. " << endl;
            }
            else
            {
                send_wake_on_lan_packet(mac_address);
            }
        }
    }
    return nullptr;
}

string get_host_name()
{
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        return string(hostname);
    }
    else
    {
        cout << "Falha no get_host_name." << endl;
        return "";
    }
}

string get_mac_address()
{
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1)
    {
        cout << "Falha no get_mac_address." << endl;
        return "";
    }

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
    {
        cout << "Falha no get_mac_address." << endl;
        close(sock);
        return "";
    }

    struct ifreq *it = ifc.ifc_req;
    const struct ifreq *const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    string mac_address = "";
    for (; it != end; ++it)
    {
        strncpy(ifr.ifr_name, it->ifr_name, IFNAMSIZ - 1);

        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0)
        {
            if (!(ifr.ifr_flags & IFF_LOOPBACK))
            {
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0)
                {
                    unsigned char *mac = reinterpret_cast<unsigned char *>(ifr.ifr_hwaddr.sa_data);
                    char mac_buffer[18];
                    snprintf(mac_buffer, sizeof(mac_buffer), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                    mac_address = mac_buffer;
                    break;
                }
            }
        }
        else
        {
            cout << "Falha no get_mac_address." << endl;
            close(sock);
            return "";
        }
    }

    close(sock);

    if (mac_address.empty())
    {
        cout << "Falha no get_mac_address." << endl;
        return "";
    }

    return mac_address;
}

string get_ip_address()
{
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    string ip_address;

    if (getifaddrs(&ifaddr) == -1)
    {
        cerr << "Falha no get_ip_address." << endl;
        return "";
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == nullptr)
            continue;

        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET)
        {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
            if (s != 0)
            {
                cerr << "Falha no get_ip_address." << endl;
                return "";
            }
            if (strcmp(ifa->ifa_name, "lo") != 0)
            {
                ip_address = host;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);

    if (ip_address.empty())
    {
        cerr << "Falha no get_ip_address." << endl;
        return "";
    }
    return ip_address;
}

string pack_discovery_sending_message()
{
    // Coleta nome do host, endereço mac e endereço IP
    string hostname = get_host_name();
    string mac_address = get_mac_address();
    string ip_address = get_ip_address();

    string message = hostname + ";" + mac_address + ";" + ip_address + ";";
    return message;
}

guest parse_payload(char* payload)
{
    guest guest_payload;
    string delimiter = ";";
    string temp_payload = payload;

    string hostname = temp_payload.substr(0, temp_payload.find(delimiter));
    temp_payload.erase(0, temp_payload.find(delimiter) + delimiter.length());

    string mac_address = temp_payload.substr(0, temp_payload.find(delimiter));
    temp_payload.erase(0, temp_payload.find(delimiter) + delimiter.length());

    string ip_address = temp_payload.substr(0, temp_payload.find(delimiter));

    guest_payload.hostname = hostname;
    guest_payload.mac_address = mac_address;
    guest_payload.ip_address = ip_address;

    return guest_payload;
}

void send_wake_on_lan_packet(string mac_address)
{
    char comando[256];
    snprintf(comando, sizeof(comando), "wakeonlan %s", mac_address.c_str());

    FILE* fp = popen(comando, "r");
    if (fp == NULL) {
        perror("Erro ao enviar WOL.");
        return;
    }
    pclose(fp);
}





