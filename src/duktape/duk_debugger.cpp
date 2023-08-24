#undef UNICODE
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include "duktape.h"
#include "duk_debugger.h"

#if defined(_MSC_VER)
#pragma comment (lib, "Ws2_32.lib")
#endif

using namespace Lumix;

namespace duk_debugger {

static SOCKET server_sock = INVALID_SOCKET;
static SOCKET client_sock = INVALID_SOCKET;
static int wsa_inited = 0;

bool init() {
	WSADATA wsa_data = {};

	int rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (rc != 0) return false;
	wsa_inited = 1;

	addrinfo hints = {};
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	addrinfo *result = NULL;

	rc = getaddrinfo("0.0.0.0", "9091", &hints, &result);
	if (rc != 0) return false;

	server_sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (server_sock == INVALID_SOCKET) goto fail;

	rc = bind(server_sock, result->ai_addr, (int) result->ai_addrlen);
	if (rc == SOCKET_ERROR) goto fail;

	rc = listen(server_sock, SOMAXCONN);
	if (rc == SOCKET_ERROR) goto fail;

	if (result) freeaddrinfo(result);
	return true;

 fail:
	if (result != NULL) {
		freeaddrinfo(result);
		result = NULL;
	}
	if (server_sock != INVALID_SOCKET) {
		closesocket(server_sock);
		server_sock = INVALID_SOCKET;
	}
	if (wsa_inited) {
		WSACleanup();
		wsa_inited = 0;
	}
	return false;
}

void disconnect() {
	if (client_sock != INVALID_SOCKET) {
		closesocket(client_sock);
		client_sock = INVALID_SOCKET;
	}
}

void finish() {
	if (client_sock != INVALID_SOCKET) {
		closesocket(client_sock);
		client_sock = INVALID_SOCKET;
	}
	if (server_sock != INVALID_SOCKET) {
		closesocket(server_sock);
		server_sock = INVALID_SOCKET;
	}
	if (wsa_inited) {
		WSACleanup();
		wsa_inited = 0;
	}
}

bool isConnected() { return client_sock != INVALID_SOCKET; }

bool tryConnect() {
	if (server_sock == INVALID_SOCKET) return 0;
	if (client_sock != INVALID_SOCKET) {
		(void) closesocket(client_sock);
		client_sock = INVALID_SOCKET;
	}

	fd_set read_fd_set;
    FD_ZERO(&read_fd_set);
    FD_SET(server_sock, &read_fd_set);
	TIMEVAL timeout;
	timeout.tv_sec = 0;
    timeout.tv_usec = 0;
	if (select(2, &read_fd_set, NULL, NULL, &timeout) > 0) {
		client_sock = accept(server_sock, NULL, NULL);
		if (client_sock == INVALID_SOCKET) return false;
		return true;
	}

	if (client_sock != INVALID_SOCKET) {
		(void) closesocket(client_sock);
		client_sock = INVALID_SOCKET;
	}
	return false;
}

duk_size_t readCallback(void *udata, char *buffer, u64 length) {
	if (client_sock == INVALID_SOCKET) return 0;
	if (length == 0) goto fail;
	if (buffer == NULL) goto fail;

	i32 ret = recv(client_sock, buffer, (int) length, 0);
	if (ret <= 0 || ret > (i32)length) {
		goto fail;
	}

	return ret;

 fail:
	if (client_sock != INVALID_SOCKET) {
		closesocket(client_sock);
		client_sock = INVALID_SOCKET;
	}
	return 0;
}

u64 writeCallback(void *udata, const char *buffer, u64 length) {
	if (client_sock == INVALID_SOCKET) return 0;
	if (length == 0) goto fail;
	if (buffer == NULL) goto fail;

	i32 ret = send(client_sock, buffer, (i32)length, 0);
	if (ret <= 0 || ret > (i32)length) goto fail;

	return ret;

 fail:
	if (client_sock != INVALID_SOCKET) {
		closesocket(INVALID_SOCKET);
		client_sock = INVALID_SOCKET;
	}
	return 0;
}

u64 peekCallback(void *udata) {
	u_long avail;

	if (client_sock == INVALID_SOCKET) return 0;

	avail = 0;
	i32 rc = ioctlsocket(client_sock, FIONREAD, &avail);
	if (rc != 0) {
		goto fail;
	} else {
		if (avail == 0) {
			return 0;
		} else {
			return 1;
		}
	}

 fail:
	if (client_sock != INVALID_SOCKET) {
		closesocket(client_sock);
		client_sock = INVALID_SOCKET;
	}
	return 0;
}

}
