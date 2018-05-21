#include "TCPServer.h"


TCPServer::TCPServer(u_int32_t ip, uint16_t port) : ip(ip), portno(port) {

	for (int i = 0; i < CLIENT_MAX; i++) {
		client_fds[i].stat = ClientData::TO_RECV;
		client_fds[i].send_len = 0;
		client_fds[i].recv_len = 0;
		client_fds[i].recv_playload = new char[ClientData::BUFFER_LEN];
		client_fds[i].send_playload = new char[ClientData::BUFFER_LEN];
	}

	onRecvCallBack = nullptr;
}


TCPServer::~TCPServer() {
}

void TCPServer::StartServer() {
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = ip;
	serv_addr.sin_port = htons(portno);


	//����sockfd
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

	int flags1;
	flags1 = fcntl(server_sockfd, F_GETFL);
	flags1 |= O_NONBLOCK;
	if (fcntl(server_sockfd, F_SETFL, flags1) == -1) {
		perror("fcntl");
		exit(1);
	}

	int nRecvBuf = 100 * 1024;         //����Ϊ100K
	if (setsockopt(server_sockfd, SOL_SOCKET, SO_RCVBUF, (const char *)&nRecvBuf, sizeof(int)) < 0) {
		perror("call to setsockopt");
		close(server_sockfd);
		exit(1);
	}


	//���õ�ַ������
	int reuse = 1;
	setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));


	//bind���ɹ�����0��������-1
	if (bind(server_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
		perror("bind");
		exit(1);
	}

	printf("����%d�˿�\n", portno);
	//listen���ɹ�����0��������-1
	if (listen(server_sockfd, QUEUE) == -1) {
		perror("listen");
		exit(1);
	}

	//printf("�ȴ��ͻ�������\n");

	int now_fd = server_sockfd;
	struct sockaddr_in client_addr;
	socklen_t length = sizeof(client_addr);

	while (1) {
		int i;
		fd_set rfds;
		int retval1;
		//int conn;

		FD_ZERO(&rfds);
		FD_SET(server_sockfd, &rfds);

		for (i = 0; i < CLIENT_MAX; i++)
			if (client_fds[i].clientfd) {
				FD_SET(client_fds[i].clientfd, &rfds);
				if (now_fd < client_fds[i].clientfd)
					now_fd = client_fds[i].clientfd;
			}
		retval1 = select(now_fd + 1, &rfds, NULL, NULL, NULL);

		if (retval1 > 0) {
			if (FD_ISSET(server_sockfd, &rfds)) {
				int conn = accept(server_sockfd, (struct sockaddr *) &client_addr, &length);
				if (conn < 0) {
					perror("connect");
					exit(1);
				}
				else {
					for (i = 0; i < CLIENT_MAX; i++)
						if (!client_fds[i].clientfd) {
							client_fds[i].clientfd = conn;
							printf("�ͻ���%d���ӳɹ�\n", i + 1);
							break;
						}
				}
				int flags;
				flags = fcntl(conn, F_GETFL);
				flags |= O_NONBLOCK;
				if (fcntl(conn, F_SETFL, flags) == -1) {
					perror("fcntl");
					exit(1);
				}

			}
		}


		for (i = 0; i < CLIENT_MAX; i++)
			if (client_fds[i].clientfd > 0) {
				if (FD_ISSET(client_fds[i].clientfd, &rfds)) {
					//TODO
					if (client_fds[i].stat == ClientData::TO_RECV)//recv
					{
						if (tcp_recv_server(client_fds[i].clientfd, client_fds[i].recv_playload,
							ClientData::BUFFER_LEN) <= 0) {
							perror("recv");
							exit(1);
						}
						onRecvCallBack(&client_fds[i]);
					}
					if (client_fds[i].stat == ClientData::TO_SEND)//send
					{
						if (tcp_send_server(client_fds[i].clientfd, client_fds[i].send_playload,
							client_fds[i].send_len) <= 0) {
							perror("send");
							exit(1);
						}

					}

				}

			}
	}


}

int TCPServer::tcp_send_server(int clientfd, const char *data, size_t len) {
	int ret;
	if (len <= 0) {
		//log->debug("invalid send recv_len");
		return -1;
	}

	do {
		ret = send(clientfd, data, len, 0);
	} while (ret < 0 && errno == EINTR);
	//log->debug("send return:{}", ret);

	int i;
	for (i = 0; i < CLIENT_MAX; i++) {
		if (client_fds[i].clientfd == clientfd)
			break;
	}
	client_fds[i].stat = ClientData::TO_RECV;
	return ret;
}


//TODO �㲥

void TCPServer::Broadcast(char *playload, size_t len)
{
	int ret = -1;
	int sock = -1;
	int so_broadcast = 1;
	struct sockaddr_in broadcast_addr; //�㲥��ַ
	struct timeval timeout;

	//�������ݱ��׽���
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		perror("create socket failed:");
		exit(0);
	}

	bzero(&broadcast_addr, sizeof(broadcast_addr));
	broadcast_addr.sin_family = AF_INET;
	broadcast_addr.sin_port = htons(BCAST_PORT);
	inet_pton(AF_INET, BCAST_IP, &broadcast_addr.sin_addr);

	printf("\nBroadcast-IP: %s\n", inet_ntoa(broadcast_addr.sin_addr));


	//Ĭ�ϵ��׽���������sock�ǲ�֧�ֹ㲥�����������׽�����������֧�ֹ㲥
	ret = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &so_broadcast,
		sizeof(so_broadcast));

	//���Է��Ͷ�ι㲥�����������Ƿ��з���������
	int times = 10;
	for (int i = 0; i < times; i++)
	{
		//�㲥���ͷ�������ַ����
		timeout.tv_sec = 2;  //��ʱʱ��Ϊ2��
		timeout.tv_usec = 0;
		ret = sendto(sock, playload, len, 0,
			(struct sockaddr*) &broadcast_addr, sizeof(broadcast_addr));
		if (ret < 0)
			continue;
		else
			break;

	}

}


int TCPServer::tcp_recv_server(int clifd, char *data, size_t len) {
	if (!data) {
		//log->debug("recv_playload is null");

		return -1;
	}

	int ret = recv(clifd, data, len, 0);
	//log->debug("read return:{}", ret);
	return ret;
}

void TCPServer::SendPacket(string id, char *playload, size_t len) {
	int i;
	for (i = 0; i < CLIENT_MAX; i++) {
		if (client_fds[i].id == id)
			break;
	}

	if (client_fds[i].send_len + len > ClientData::BUFFER_LEN) {
		//log->critical("send buffer will overflow!");
		return;
	}
	memcpy(client_fds[i].send_playload + client_fds[i].send_len, playload, len);
	client_fds[i].send_len += len;
	client_fds[i].stat = ClientData::TO_SEND;
}

void TCPServer::setOnRecvCallBack(void(*callBack)(ClientData *)) {
	onRecvCallBack = callBack;
}

