#include "globals.cpp"

// Funções do participante
static void guest_program();
void* guest_discovery_service(void *arg);
void* guest_monitoring_service(void *arg);
string get_host_name();
string get_mac_address();
string get_ip_address();

// Funções do manager
static void manager_program();
void* manager_discovery_service(void *arg);
void* manager_monitoring_service(void *arg);
void* manager_interface_service(void *arg);

// cout << "oi" << endl;
// cout << "hello" << endl;

int main(int argc, char **argv)
{
    cout << "./sleep_server OR ./sleep_server manager" << endl;

    // ./sleep_server
    if (argc == 1)
    {
        //guest_program();
        manager_program();
    }
    // ./sleep_server manager
    else if (argc == 2 && strcmp(argv[1], "manager") == 0)
    {
        manager_program();
    }
}

static void guest_program()
{
    pthread_t thread_discovery, thread_monitoring;
    pthread_attr_t attributes_thread_discovery, attributes_thread_monitoring;

    // inicializando os atributos das threads
    if (pthread_attr_init(&attributes_thread_discovery) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação dos atributos da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_attr_init(&attributes_thread_monitoring) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação dos atributos da threads de monitoramento." << endl;
        exit(1);
    }

    // criando as threads
    if (pthread_create(&thread_discovery, &attributes_thread_discovery, guest_discovery_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_create(&thread_monitoring, &attributes_thread_monitoring, guest_monitoring_service, nullptr) != 0)
    {
        cout << "PARTICIPANTE: Erro na criação da threads de monitoramento." << endl;
        exit(1);
    }

    // thread pai espera elas terminarem
    pthread_join(thread_discovery, nullptr);
    pthread_join(thread_monitoring, nullptr);

    // destruindo atributos
    pthread_attr_destroy(&attributes_thread_discovery);
    pthread_attr_destroy(&attributes_thread_monitoring);
}

static void manager_program()
{
    pthread_t thread_discovery, thread_monitoring, thread_interface;
    pthread_attr_t attributes_thread_discovery, attributes_thread_monitoring, attributes_thread_interface;

    // inicializando os atributos das threads
    if (pthread_attr_init(&attributes_thread_discovery) != 0)
    {
        cout << "MANAGER: Erro na criação dos atributos da thread de descoberta." << endl;
        exit(1);
    }
    if (pthread_attr_init(&attributes_thread_monitoring) != 0)
    {
        cout << "MANAGER: Erro na criação dos atributos da thread de monitoramento." << endl;
        exit(1);
    }
    if (pthread_attr_init(&attributes_thread_interface)!= 0)
    {
        cout << "MANAGER: Erro na criação dos atributos da thread da interface." << endl;
        exit(1);
    }

    // criando as threads
    //if (pthread_create(&thread_discovery, &attributes_thread_discovery, manager_discovery_service , nullptr) != 0)
    //{
    //    cout << "MANAGER: Erro na criação da thread de descoberta." << endl;
    //    exit(1);
    //}
    //if (pthread_create(&thread_monitoring, &attributes_thread_monitoring, manager_monitoring_service, nullptr) != 0)
    //{
    //    cout << "MANAGER: Erro na criação da thread de monitoramento." << endl;
    //    exit(1);
    //}
    if (pthread_create(&thread_interface, &attributes_thread_interface, manager_interface_service, nullptr) != 0)
    {
        cout << "MANAGER: Erro na criação da thread da interface." << endl;
        exit(1);
    }

    // thread pai espera elas terminarem
    //pthread_join(thread_discovery, nullptr);
    //pthread_join(thread_monitoring, nullptr);
    pthread_join(thread_interface, nullptr);

    // destruindo atributos
    pthread_attr_destroy(&attributes_thread_discovery);
    pthread_attr_destroy(&attributes_thread_monitoring);
    pthread_attr_destroy(&attributes_thread_interface);
}

// Cliente -> Envia pacotes do tipo SLEEP SERVICE DISCOVERY para as estações por broadcast até ser respondido.
void* guest_discovery_service(void *arg)
{
    // Criando o socket
    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        cout << "PARTICIPANTE: Erro na criação do socket do servidor de descoberta." << endl;
        exit(1);
    }

    // Setando o socket para a opção broadcast
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int)) < 0)
    {
        cout << "PARTICIPANTE: Erro ao setar o modo broadcast do socket do servidor de descoberta." << endl;
        exit(1);
    }

    // Configurando o endereço do servidor de descoberta
    struct sockaddr_in discovery_server_address;
    discovery_server_address.sin_family = AF_INET;
	discovery_server_address.sin_port = htons(PORT_DISCOVERY_SERVICE);
	discovery_server_address.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    socklen_t discovery_server_address_len = sizeof(struct sockaddr_in);

    // Coleta nome do host, endereço mac e endereço IP
    string hostname = get_hostname();
    string mac_address = get_mac_address();
    string ip_address = get_ip_address();

    // Faz uma mensagem com essas informações para ser enviada
    size_t  message_len = sizeof(packet);
    string message = hostname + ";" + mac_address + ";" + ip_address;
    packet* discover_message = new packet();
    discover_message->type = SLEEP_SERVICE_DISCOVERY;
    strcpy(discover_message->payload, message);

    // Inicializando o espaço de memória que vai receber a mensagem de resposta do manager.
    packet* manager_message = new packet();

    // Inicializando o espaço de memória que vai receber o endereço do manager.
    struct sockaddr_in manager_server_address;
    socklen_t manager_server_address_len = sizeof(struct sockaddr_in);

    // Enviando mensagens a todo tempo
    while(1)
    {
        if (sendto(socket_descriptor, discover_message, message_len, 0,(struct sockaddr *) &discovery_server_address, &discovery_server_address_len) < 0)
        {
            cout << "PARTICIPANTE: Erro ao enviar mensagem no servidor de descoberta." << endl;
        }
        // Esperando resposta a todo tempo
        if (recvfrom(socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &manager_server_address, &manager_server_address_len) < 0)
        {
            cout << "PARTICIPANTE: Erro ao enviar mensagem ao manager pelo servidor de descoberta." << endl;
        }
        else
        {
            // Até o manager responder
            if (manager_message.type == SLEEP_SERVICE_DISCOVERY)
            {
                break;
            }
        }
    }
    free(hostname);
    free(mac_address);
    free(ip_address);
    free(discover_message);

    close(socket_descriptor);
    return nullptr;
}

// Servidor -> Recebendo e respondendo pacotes do tipo SLEEP STATUS REQUEST para o manager.
void* guest_monitoring_service(void *arg)
{
    // Criando o socket
    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        cout << "PARTICIPANTE: Erro na criação do socket do servidor de monitoramento." << endl;
        exit(1);
    }

    // Setando o socket para a opção REUSEADDR
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        cout << "PARTICIPANTE: Erro ao setar o modo REUSEADDR do socket do servidor de monitoramento." << endl;
        exit(1);
    }

    // Configurando o endereço do servidor de monitoramento
    struct sockaddr_in monitoring_server_address;
    monitoring_server_address.sin_family = AF_INET;
	monitoring_server_address.sin_port = htons(PORT_MONITORING_SERVICE);
	monitoring_server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t monitoring_server_address_len = sizeof(struct sockaddr_in);

    // Endereçando o socket ao servidor de monitoramento
    if (bind(socket_descriptor, (struct sockaddr*) &monitoring_server_address, monitoring_server_address_len) < 0)
    {
        cout << "PARTICIPANTE: Erro ao endereçar o socket do servidor de monitoramento." << endl;
        exit(1);
    }

    size_t  message_len = sizeof(packet);
    // Inicializando o espaço de memória que vai receber a mensagem do manager.
    packet* manager_message = new packet();
    // Inicializando o espaço de memória da mensagem em resposta ao manager.
    packet* awake_message = new packet();
    awake_message->type = SLEEP_STATUS_REQUEST;
    strcpy(awake_message->payload, STATUS_AWAKE);

    // Inicializando o espaço de memória que vai receber o endereço do manager.
    struct sockaddr_in manager_server_address;
    socklen_t manager_server_address_len = sizeof(struct sockaddr_in);

    // Recebendo mensagens a todo tempo
    while (1)
    {
        if (recvfrom(socket_descriptor, manager_message, message_len, 0,(struct sockaddr *) &manager_server_address, &manager_server_address_len) < 0)
        {
            cout << "PARTICIPANTE: Erro ao receber mensagem do manager no servidor de monitoramento." << endl;
        }
        // Se manager mandou mensagem de monitoramento, responde
        else
        {
            if (message.type == SLEEP_STATUS_REQUEST)
            {
                if (sendto(socket_descriptor, awake_message, message_len, 0,(struct sockaddr *) &manager_server_address, &manager_server_address_len) < 0)
                {
                    cout << "PARTICIPANTE: Erro ao enviar mensagem ao manager pelo servidor de monitoramento." << endl;
                }
            }
        }
    }
    close(socket_descriptor);
    free(manager_message);
    free(awake_message);

    return nullptr;
}

// Servidor -> Recebendo e respondendo pacotes do tipo SLEEP SERVICE DISCOVERY.
void* manager_discovery_service(void *arg)
{
    // Criando o socket
    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        cout << "MANAGER: Erro na criação do socket do servidor de descoberta." << endl;
        exit(1);
    }

    // Setando o socket para a opção broadcast
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int)) < 0)
    {
        cout << "MANAGER: Erro ao setar o modo broadcast do socket do servidor de descoberta." << endl;
        exit(1);
    }

    // Configurando o endereço do servidor de descoberta
    struct sockaddr_in discovery_server_address;
    discovery_server_address.sin_family = AF_INET;
	discovery_server_address.sin_port = htons(PORT_DISCOVERY_SERVICE);
	discovery_server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t discovery_server_address_len = sizeof(struct sockaddr_in);

    // Endereçando o socket ao servidor de descoberta
    if (bind(socket_descriptor, (struct sockaddr*) &discovery_server_address, discovery_server_address_len) < 0)
    {
        cout << "MANAGER: Erro ao endereçar o socket do servidor de descoberta." << endl;
        exit(1);
    }

    size_t  message_len = sizeof(packet);
    // Inicializando o espaço de memória que vai receber a mensagem do participante.
    packet* guest_message = new packet();
    // Inicializando o espaço de memória da mensagem em resposta ao participante.
    packet* discovered_message = new packet();
    discovered_message->type = SLEEP_SERVICE_DISCOVERY;
    strcpy(awaken_message->payload, GUEST_DISCOVERED);

    // Inicializando o espaço de memória que vai receber o endereço do guest.
    struct sockaddr_in manager_server_address;
    socklen_t manager_server_address_len = sizeof(struct sockaddr_in);

    // Recebendo mensagens a todo tempo
    while (1)
    {
        if (recvfrom(socket_descriptor, guest_message, message_len, 0,(struct sockaddr *) &manager_server_address, &manager_server_address_len) < 0)
        {
            cout << "MANAGER: Erro ao receber mensagem de participante no servidor de descoberta." << endl;
            continue;
        }
        else
        {
            // Se encontrou um participante, envia resposta
            if (guest_message.type == SLEEP_SERVICE_DISCOVERY)
            {
                if (sendto(socket_descriptor, discovered_message, message_len, 0,(struct sockaddr *) &manager_server_address, &manager_server_address_len) < 0)
                {
                    cout << "MANAGER: Erro ao enviar mensagem a participante pelo servidor de descoberta." << endl;
                }
                // AQUI DEVE CHAMAR A FUNÇÃO PARA ATUALIZAR ENTRADA DA INTERFACE // guest_message.payload = "hostname;mac_address;ip_address"
                // ADICIONA PARTICIPANTE NA LISTA DE PARTICIPANTES
            }
        }
    }
    close(socket_descriptor);
    free(guest_message);
    free(discovered_message);

    return nullptr;
}

// Cliente -> Enviando pacotes do tipo SLEEP STATUS REQUEST para as estações já conhecidas e processa resposta.
void* manager_monitoring_service(void *arg)
{
    // Criando o socket
    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor < 0)
    {
        cout << "MANAGER: Erro na criação do socket do servidor de monitoramento." << endl;
        exit(1);
    }

    // Setando o socket para a opção REUSEADDR
    if (setsockopt(socket_descriptor, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        cout << "PARTICIPANTE: Erro ao setar o modo REUSEADDR do socket do servidor de monitoramento." << endl;
        exit(1);
    }

    // Configurando o endereço do servidor de monitoramento
    struct sockaddr_in monitoring_server_address;
    monitoring_server_address.sin_family = AF_INET;
	monitoring_server_address.sin_port = htons(PORT_MONITORING_SERVICE);
	monitoring_server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t monitoring_server_address_len = sizeof(struct sockaddr_in);

    // Endereçando o socket ao servidor de monitoramento
    if (bind(socket_descriptor, (struct sockaddr*) &monitoring_server_address, monitoring_server_address_len) < 0)
    {
        cout << "PARTICIPANTE: Erro ao endereçar o socket do servidor de monitoramento." << endl;
        exit(1);
    }

    size_t  message_len = sizeof(packet);
    // Inicializando o espaço de memória que vai receber a mensagem do guest.
    packet* guest_message = new packet();
    // Inicializando o espaço de memória da mensagem para o guest.
    packet* manager_message = new packet();
    manager_message->type = SLEEP_STATUS_REQUEST;
    strcpy(manager_message->payload, STATUS_REQUEST);

    // duas execuções:
    // envia manager_message para cada guest já conhecido;
    // fica esperando resposta até certo tempo, se não chegou, atualiza tabela para sleep.

    // Recebendo mensagens a todo tempo
    while (1)
    {

    }

    return nullptr;
}

// Atualizando a interface.
void* manager_interface_service(void *arg)
{
    guestTable guests = new guestTable();
    guests.addGuest(new guest("oi","nao","to","aqui"));
    guests.printTable();
    return nullptr;
}

string get_host_name()
{

}

string get_mac_address()
{

}

string get_ip_address()
{


}


