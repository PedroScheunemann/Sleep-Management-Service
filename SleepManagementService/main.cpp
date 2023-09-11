#include "globals.cpp"

string g_my_ip_address;
string g_my_mac_address;
string g_my_hostname;

pthread_mutex_t g_mutex_table = PTHREAD_MUTEX_INITIALIZER;
guestTable g_my_guests;

pthread_mutex_t g_mutex_election_occurring = PTHREAD_MUTEX_INITIALIZER;
int g_election_occurring = 0;

pthread_mutex_t g_mutex_guest_out = PTHREAD_MUTEX_INITIALIZER;
int g_out_of_service = 0;

// Funções do participante
static void guest_program();
void* guest_election_discovery_service(void *arg);
void* guest_monitoring_service(void *arg);
void* guest_interface_service(void *arg);
string get_hostname();
string get_mac_address();
string get_ip_address();
string pack_my_info();

// Funções do manager
static void manager_program();
void* manager_discovery_service(void *arg);
void* manager_monitoring_service(void *arg);
void* manager_interface_service(void *arg);
guest parse_payload(char* payload);
void send_wake_on_lan_packet(string mac_address);

// Eleição
void discovery_service();
void election_service();
void setup_my_info();

int main(int argc, char **argv)
{
    setup_my_info();
    discovery_service();
}

void setup_my_info()
{
    // Setando informações globais
    g_my_hostname = get_hostname();
    g_my_ip_address = get_ip_address();
    g_my_mac_address = get_mac_address();

    // Me adicionando na tabela de participantes
    guest new_guest;
    new_guest.hostname = g_my_hostname;
    new_guest.ip_address = g_my_ip_address;
    new_guest.mac_address = g_my_mac_address;
    new_guest.status = "awake";

    g_my_guests.version = 0;
    g_my_guests.addGuest(new_guest);
    g_my_guests.printTable();
}

// CLIENTE: Envia pacotes do tipo SLEEP_SERVICE_DISCOVERY para as estações por broadcast e espera ser respondido pelo manager.
// Se manager responder, atualiza a tabela local com a que ele enviou.
// Se manager não responder, inicia uma eleição.
void discovery_service()
{
    // Criando o descritor do socket
    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        cout << "Erro na criação do socket [DESCOBERTA]." << endl;
        exit(1);
    }

    // Setando a opção de broadcast do socket (habilitando o envio de broadcast messages)
    int enable_broadcast = 1;
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_BROADCAST, &enable_broadcast, sizeof(int)) < 0)
    {
        cout << "Erro ao setar o modo broadcast do socket [DESCOBERTA]." << endl;
        close(socket_descriptor);
        exit(1);
    }

    // Setando um timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        cout << "Erro ao setar o timeout do socket [DESCOBERTA]." << endl;
        close(socket_descriptor);
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
    strcpy(sending_message->payload, pack_my_info().c_str());
    ////////////////////////////////////////////////////////////////////////////////////////////

    // Enviando mensagem para todo mundo
    if (sendto(socket_descriptor, sending_message, message_len, 0,(struct sockaddr *) &sending_address, address_len) < 0)
    {
        cout << "Erro ao enviar mensagem de descoberta." << endl;
        close(socket_descriptor);
        exit(1);
    }
    // Esperando resposta do manager chegar
    if (recvfrom(socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &manager_address, &address_len) < 0)
    {
        close(socket_descriptor);
        election_service();                    // Se não chegou, começa uma eleição
    }
    else
    {
        close(socket_descriptor);
        g_my_guests = manager_message->guests; // Se chegou, atualiza a tabela local
        g_my_guests.printTable();
        guest_program();                       // Roda como participante
    }
}

// CLIENTE: Envia pacotes do tipo ELECTION para as estações já conhecidas com IP maior e espera ser respondido.
// Se ninguém responder, se auto-intitula líder.
// Se alguém responder, espera resposta com o líder.
void election_service()
{
    pthread_mutex_lock(&g_election_occurring);
    g_election_occurring = 1;
    pthread_mutex_unlock(&g_election_occurring);

    // Criando o descritor do socket
    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        cout << "Erro na criação do socket [ELEIÇÃO]." << endl;
        exit(1);
    }

    // Setando um timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        cout << "Erro ao setar o timeout do socket [ELEIÇÃO]." << endl;
        close(socket_descriptor);
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO ENDEREÇOS //////////////////////////////////
    struct sockaddr_in sending_address;        // endereço que receberá cada participante para onde mandaremos e receberemos mensagem
    socklen_t address_len = sizeof(struct sockaddr_in);

    sending_address.sin_family = AF_INET;
    ////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////// CONFIGURANDO MENSAGEM ///////////////////////////////////
    packet* sending_message = new packet();    // mensagem que será enviada para os participantes
    packet* guest_message = new packet();      // mensagem de resposta que será recebida dos participantes
    size_t  message_len = sizeof(packet);

    sending_message->type = ELECTION;
    ////////////////////////////////////////////////////////////////////////////////////////////


    int election_ended = 0, waiting_for_coordinator;
    while (!election_ended)
    {
        waiting_for_coordinator = 0;

        // Pega a lista de IPs dos participantes
        pthread_mutex_lock(&g_mutex_table);
        vector<string> guests_list_ip_adresses = g_my_guests.returnGuestsIpAddressList();
        pthread_mutex_unlock(&g_mutex_table);

        // Itera todos os IPs
        for(vector<string>::iterator it = guests_list_ip_adresses.begin(); it != guests_list_ip_adresses.end(); ++it)
        {
            if (stoi(*it) > stoi(g_my_ip_address))  // Envia mensagem de eleição para todos os IPs maiores
            {
                sending_address.sin_port = htons(PORT_ELECTION_SERVICE);
                inet_aton(it->c_str(), &sending_address.sin_addr);  // Para (PORT_ELECTION_SERVICE, IP do participante)

                if (sendto(socket_descriptor, sending_message, message_len, 0,(struct sockaddr *) &sending_address, address_len) < 0) {
                    cout << "ELECTION: Erro ao enviar mensagem para algum participante." << endl;
                    close(socket_descriptor);
                    exit(1);
                }

                // Esperando resposta chegar até timeout
                if (recvfrom(socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &sending_address, &address_len) >= 0)
                {
                    // Se a resposta que chegou foi answer
                    if (guest_message->type == ANSWER)
                    {
                        // Não posso mais me denominar como líder, devo esperar resposta de coordinator
                        waiting_for_coordinator = 1;
                        break;
                    }
                }
            }
        }
        // aqui eu devo: setar um socket servidor para esperar uma resposta de coordinator. se ela chegar: rodo guestprogram(). senão: reinicia o loop
        if (waiting_for_coordinator)
        {
            // Se recebeu mensagem de coordinator, eleição termina: election_ended = 1;
            // Senão, eleição continua.
        }
        else
        {
            election_ended = 1;
        }
    }
    close(socket_descriptor);

    // Se terminou a eleição enquanto estava esperando por coordinator, então sou um participante
    if(waiting_for_coordinator)
    {
        guest_program();
    }
    // Senão, sou o novo líder
    else
    {
        manager_program();
    }
}

static void guest_program()
{
    pthread_t thread_election_discovery, thread_monitoring, thread_interface;

    if (pthread_create(&thread_election_discovery, nullptr, guest_election_discovery_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação da thread de descoberta de eleição." << endl;
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
    pthread_join(thread_election_discovery, nullptr);
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

// SERVIDOR: Recebendo e respondendo pacotes do tipo ELECTION.
// Quando receber mensagem de election, envia mensagem do tipo answer, e inicia uma eleição, caso não estiver em uma.
void* guest_election_discovery_service(void *arg)
{
    // Criando o descritor do socket do participante
    int guest_socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (guest_socket_descriptor < 0)
    {
        cout << "PARTICIPANTE: Erro na criação do socket [DESCOBERTA]." << endl;
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO ENDERE�OS //////////////////////////////////
    struct sockaddr_in guest_address;      // endereço local do guest
    struct sockaddr_in elector_address;    // endereço de onde vem o pacote (que será do eleitor)
    socklen_t address_len = sizeof(struct sockaddr_in);

    guest_address.sin_family = AF_INET;
	guest_address.sin_port = htons(PORT_ELECTION_SERVICE);  // recebe na porta PORT_ELECTION_SERVICE
	guest_address.sin_addr.s_addr = htonl(INADDR_ANY);
    ////////////////////////////////////////////////////////////////////////////////////////////

    // Endereçando o socket
    if (bind(guest_socket_descriptor, (struct sockaddr*) &guest_address, address_len) < 0)
    {
        cout << "PARTICIPANTE: Erro ao endereçar o socket [DESCOBERTA]." << endl;
        close(guest_socket_descriptor);
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO MENSAGEM ///////////////////////////////////
    packet* guest_message = new packet();    // mensagem de resposta que será enviada pelo participante
    packet* elector_message = new packet();  // mensagem que será recebida do eleitor
    size_t  message_len = sizeof(packet);

    guest_message->type = ANSWER;
    ////////////////////////////////////////////////////////////////////////////////////////////


    // Espera receber mensagem de algum participante
    if (recvfrom(guest_socket_descriptor, elector_message, message_len, 0,(struct sockaddr *) &elector_address, &address_len) < 0)
    {
        cout << "PARTICIPANTE: Erro ao receber mensagem de eleitor [DESCOBERTA]." << endl;
        close(guest_socket_descriptor);
        exit(1);
    }
    else
    {
        if (elector_message->type == ELECTION)
        {
            // Envia mensagem de answer
            if (sendto(guest_socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &elector_address, address_len) < 0)
            {
                cout << "PARTICIPANTE: Erro ao enviar mensagem de resposta [DESCOBERTA]." << endl;
                close(guest_socket_descriptor);
                exit(1);
            }

            pthread_mutex_lock(&g_election_occurring);
            election_occurring = g_election_occurring;
            pthread_mutex_unlock(&g_election_occurring);

            // Inicia uma eleição, caso já não estiver em uma
            if (!election_occurring)
            {
                close(guest_socket_descriptor);
                election_service();
            }
        }
    }
    close(guest_socket_descriptor);
    return nullptr;
}

// SERVIDOR: Recebendo e respondendo pacotes do tipo SLEEP_STATUS_REQUEST para o manager.
// Se receber mensagem, atualiza a tabela local com a que ele enviou.
// Se não receber dentro de um timeout, inicia uma eleição, caso já não estiver em uma.
void* guest_monitoring_service(void *arg)
{
    // Criando o descritor do socket do participante
    int guest_socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (guest_socket_descriptor < 0)
    {
        cout << "PARTICIPANTE: Erro na criação do socket [MONITORAMENTO]." << endl;
        exit(1);
    }

    // Setando um timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(guest_socket_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        cout << "PARTICIPANTE: Erro ao setar o timeout do socket [MONITORAMENTO]." << endl;
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
        cout << "PARTICIPANTE: Erro ao endereçar o socket [MONITORAMENTO]." << endl;
        close(guest_socket_descriptor);
        exit(1);
    }

    ////////////////////////////////// CONFIGURANDO MENSAGEM ///////////////////////////////////
    packet* manager_message = new packet();    // mensagem que será recebida do manager
    packet* guest_message = new packet();      // mensagem de resposta que será enviada pelo guest
    size_t  message_len = sizeof(packet);
    ////////////////////////////////////////////////////////////////////////////////////////////

    int manager_out = 0, out_of_service, election_occurring;
    while (1)
    {
        // Espera receber mensagem do manager até timeout
        if (recvfrom(guest_socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &manager_address, &address_len) < 0)
        {
            manager_out = 1;
            break;
        }
        else
        {
            // Se manager mandou tabela atualizada, faz uma cópia profunda
            pthread_mutex_lock(&g_mutex_table);
            if (manager_message->guests->version > g_my_guests.version)
            {
                g_my_guests = manager_message->guests;
                g_my_guests.printTable();
            }
            pthread_mutex_unlock(&g_mutex_table);

            // Se manager mandou mensagem de monitoramento, responde
            if (manager_message->type == SLEEP_STATUS_REQUEST)
            {
                pthread_mutex_lock(&g_out_of_service);
                out_of_service = g_out_of_service;
                pthread_mutex_unlock(&g_out_of_service);

                // Se o participante quer sair do serviço, a mensagem será avisando que quer sair
                if (out_of_service)
                {
                    guest_message->type = STATUS_QUIT;
                }
                // Senão, a mensagem será avisando que está acordado
                else
                {
                    guest_message->type = STATUS_AWAKE;
                }

                // Envia a mensagem para o manager
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

    pthread_mutex_lock(&g_election_occurring);
    election_occurring = g_election_occurring;
    pthread_mutex_unlock(&g_election_occurring);

    // Se manager não respondeu, inicia uma eleição, caso já não estiver em uma
    if (manager_out && !election_occurring)
    {
        election_service();
    }

    return nullptr;
}

// Fica processando a entrada do usuário pra ver se vai vir um pedido de EXIT.
// Se chegar, atualiza variável que participante quer sair.
void* guest_interface_service(void *arg)
{
    string input;
    cout << "To quit the service type EXIT." << endl;
    cin >> input;
    if (input == "EXIT")
    {
        // Garantindo acesso exclusivo a variável current_guest_is_out
        pthread_mutex_lock(&g_mutex_guest_out);
        g_out_of_service = 1;
        pthread_mutex_unlock(&g_mutex_guest_out);
    }
    return nullptr;
}

// SERVIDOR: Recebendo e respondendo pacotes do tipo SLEEP_SERVICE_DISCOVERY que chegaram através de broadcast.
// Quando receber mensagem, atualiza a tabela local com o novo participante e envia mensagem de confirmação.
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

    manager_message->type = GUEST_DISCOVERED;
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
            // Se encontrou um participante
            if (guest_message->type == SLEEP_SERVICE_DISCOVERY)
            {
                // Adiciona na tabela de paticipantes
                guest new_guest = parse_payload(guest_message->payload);
                new_guest.status = "awake";
                // Garantindo acesso exclusivo a tabela
                pthread_mutex_lock(&g_mutex_table);
                g_my_guests.version += 1;
                g_my_guests.addGuest(new_guest);
                g_my_guests.printTable();
                manager_message->guests = g_my_guests;  // Atualiza a mensagem enviada com a tabela
                pthread_mutex_unlock(&g_mutex_table);

                // Envia resposta de confirmação, com a tabela
                if (sendto(manager_socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &guest_address, address_len) < 0)
                {
                    cout << "MANAGER: Erro ao enviar mensagem a participante [DESCOBERTA]." << endl;
                    close(manager_socket_descriptor);
                    exit(1);
                }
            }
        }
    }
    close(manager_socket_descriptor);
    return nullptr;
}

// CLIENTE: Enviando pacotes do tipo SLEEP_STATUS_REQUEST para as estações já conhecidas e processando a resposta.
// Envia mensagem para cada participante, e quando receber resposta, atualiza a tabela local de acordo.
void* manager_monitoring_service(void *arg)
{
    // Criando o descritor do socket do manager
    int manager_socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (manager_socket_descriptor < 0)
    {
        cout << "MANAGER: Erro na criação do socket [MONITORAMENTO]." << endl;
        exit(1);
    }

    // Setando um timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
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
    ////////////////////////////////////////////////////////////////////////////////////////////

    while (1)
    {
        // Pega a lista de IPs dos participantes
        pthread_mutex_lock(&g_mutex_table);
        vector<string> guests_list_ip_adresses = g_my_guests.returnGuestsIpAddressList();
        pthread_mutex_unlock(&g_mutex_table);

        // Envia mensagens para todos os IPs
        for(vector<string>::iterator it = guests_list_ip_adresses.begin(); it != guests_list_ip_adresses.end(); ++it)
        {
            guest_address.sin_port = htons(PORT_MONITORING_SERVICE);
            inet_aton(it->c_str(), &guest_address.sin_addr);  // envia para (PORT_MONITORING_SERVICE, IP do participante)

            manager_message->guests = g_my_guests;  // Deve estar dentro do loop, pois g_my_guests pode ser atualizada a cada mensagem enviada
            if (sendto(manager_socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &guest_address, address_len) < 0) {
                cout << "MANAGER: Erro ao enviar mensagem para algum participante." << endl;
                close(manager_socket_descriptor);
                exit(1);
            }

            // Esperando resposta chegar
            if (recvfrom(manager_socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &guest_address, &address_len) < 0)
            {
                // Se resposta não chegou
                pthread_mutex_lock(&g_mutex_table);
                if (g_my_guests.getGuestStatus(*it) == "awake")
                {
                    g_my_guests.version += 1;
                    g_my_guests.guestSlept(*it); // Atualiza status do participante para "asleep"
                    g_my_guests.printTable();    // e printa
                }
                pthread_mutex_unlock(&g_mutex_table);
            }
            else
            {
                // Se foi avisando que está acordado
                if (guest_message->type ==  STATUS_AWAKE)
                {
                    pthread_mutex_lock(&g_mutex_table);
                    if (g_my_guests.getGuestStatus(*it) == "asleep")
                    {
                        g_my_guests.version += 1;
                        g_my_guests.wakeGuest(*it); // Atualiza status do participante para "awake", caso estivesse em "asleep"
                        g_my_guests.printTable();   // e printa
                    }
                    pthread_mutex_unlock(&g_mutex_table);
                }
                // Se foi avisando que está saindo
                if (guest_message->type == STATUS_QUIT)
                {
                    pthread_mutex_lock(&g_mutex_table);
                    g_my_guests.version += 1;
                    g_my_guests.deleteGuest(*it);   // Remove participante da tabela
                    g_my_guests.printTable();       // e printa
                    pthread_mutex_unlock(&g_mutex_table);
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
            string mac_address = g_my_guests.returnGuestMacAddress(hostname);
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

string get_hostname()
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

string pack_my_info()
{
    string message = g_my_hostname + ";" + g_my_mac_address + ";" + g_my_ip_address + ";";
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





