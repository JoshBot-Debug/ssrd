#pragma once

#include <arpa/inet.h>
#include <iostream>
#include <regex>
#include <string>

#include "Constant.h"

struct Client {
  std::string destination;
  std::string username;
  std::string ip;
  uint16_t port = DEFAULT_PORT;

  static bool isValidUsername(const std::string &user) {
    static const std::regex re("^[A-Za-z0-9_-]+$");
    return !user.empty() && std::regex_match(user, re);
  }

  static bool isValidIP(const std::string &ip) {
    sockaddr_in sa4{};
    sockaddr_in6 sa6{};
    return inet_pton(AF_INET, ip.c_str(), &(sa4.sin_addr)) == 1 ||
           inet_pton(AF_INET6, ip.c_str(), &(sa6.sin6_addr)) == 1;
  }

  static bool isValidPort(const std::string &portStr, uint16_t &portOut) {
    if (portStr.empty())
      return false;
    for (char c : portStr)
      if (!isdigit(c))
        return false;
    long val = std::stol(portStr);
    if (val < 1 || val > 65535)
      return false;
    portOut = static_cast<uint16_t>(val);
    return true;
  }

  void fromString(const std::string &s) {
    auto at = s.find('@');
    auto colon = s.find(':', at + 1);

    if (at == std::string::npos)
      throw std::runtime_error("Invalid format, expected <username>@<ip-address>[:<port>]");

    std::string portStr;

    if (colon == std::string::npos) {
      ip = s.substr(at + 1);
    } else {
      ip = s.substr(at + 1, colon - (at + 1));
      portStr = s.substr(colon + 1);
    }

    if (!isValidUsername(username = s.substr(0, at)))
      throw std::runtime_error("Invalid username");

    if (!isValidIP(ip))
      throw std::runtime_error("Invalid IP address");

    if (!portStr.empty() && !isValidPort(portStr, port))
      throw std::runtime_error("Invalid port number");
  }
};
