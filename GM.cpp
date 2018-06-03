﻿#include <sstream>
#include "GM.h"

using namespace group_sig;

/**
 * \brief 群管理员GM获取下列值
 * 
 * 			RSA公钥对 (n, b)
 * 			
 *			循环群G，g为生成元，n为G的阶
 *			
 *			系统安全性参数a,λ(群成员私钥长度),ε
 *			
 *			公开参数 (n, e, G, g, a, λ, ε)
 *			
 *			群管理员保存RSA私钥对(n, d)???
 */


void GM::init() {
    Log = get("console");
//	_ an RSA public key _n;e_,
//			_ a cyclic group G = hgi of order n in which computing discrete logarithms is
//	infeasible _e.g. G could be a subgroup of Z _ , for a prime p with nj_p , 1__,
//			p
//	_ an element a 2 Z _ _a should be of large multiplicative order modulo both
//	n
//	prime factors of n_, and
//	_ an upper bound _ on the length of the secret keys and a constant _ _ 1
//	_these parameters are required for the SKLOGLOG signatures_
    G = Cryptography::findPrime(512);
    g = Cryptography::findPrimitiveRoot(G);
    n = rsa_n = G - 1;// 群的阶和rsa的n
    rsa_b = findRandomInZn(n);
    rsa_a = InvMod(rsa_b, n);

    a = findRandomInZn(n);//TODO doesn't not know a's meaning
    epsilon = 5;
}

public_para GM::getPublicPara() const {
    return {
            rsa_n,
            rsa_b,
            G, g, a, lambda, epsilon
    };
}

ZZ GM::verify(string id, string msg) {
    Log->info("Verifying member identity...");
    cspair p;
    stringstream stream(msg);
    string token;
    stream >> token;
    // TODO decript with rsa_a
    ZZ y = Cryptography::stringToNumber(token, false);
    stream >> token;
    ZZ z = Cryptography::stringToNumber(token, false);
    // JoinGroupMsg y和z的合法性
    if (z != PowerMod(g, y, rsa_n)) {
        Log->critical("y,z inconsistent");
    }

    //验证Alice知道x
    stream >> token;
    ZZ yy = Cryptography::stringToNumber(token, false);
    stream >> token;
    ZZ aa = Cryptography::stringToNumber(token, false);
    stream >> token;
    p.c = Cryptography::stringToNumber(token, false);
    stream >> token;
    p.s.push_back(Cryptography::stringToNumber(token, false));
    if (!SKLOGver(psk, yy, aa, p)) {
        Log->critical("SKLOG verify failed!");
    }

    // GM保存(y, z)用于日后打开群签名
    info.push_back({id, y, z});
    //  GM生成Alice的成员证书 v = (y + 1) ^ d (mod n)
    ZZ v = PowerMod(y + 1, rsa_a, rsa_n);
    return v;
}

string GM::open(ZZ gg, ZZ zz) {
    for (auto i : info) {
        if (PowerMod(gg, i.y, rsa_n) == zz)
            return i.id;
    }
    string s;
    return "";
}


bool GM::SKLOGver(const ZZ &m, const ZZ &y, const ZZ &g, const cspair &p) const {
    Log->info("SKLOGver");
    ZZ temp = MulMod(PowerMod(g, p.s[0], rsa_n), PowerMod(y, p.c, rsa_n), rsa_n);
    Log->debug("SKLOGver/c: {}\ns: {}", Cryptography::numberToString(p.c, false),
               Cryptography::numberToString(p.s[0], false));
    Log->debug("SKLOGver/g^r: {}", Cryptography::numberToString(temp, false));

    string concatStr = Cryptography::numberToString(m, false) + Cryptography::numberToString(y, false) +
                       Cryptography::numberToString(g, false) +
                       Cryptography::numberToString(temp, false);
    Log->debug("SKLOGver\nm: {}\ny: {}\ng: {}", Cryptography::numberToString(m, false),
               Cryptography::numberToString(y, false), Cryptography::numberToString(g, false));


    size_t n = h(concatStr);

    ZZ cc = conv<ZZ>(n);
    Log->debug("SKLOGver/cc: {}", Cryptography::numberToString(cc, false));
    Log->debug("SKLOGver/p.c: {}", Cryptography::numberToString(p.c, false));
    return cc == p.c != 0;
}


string GM::getKeyChain() {
    stringstream msg;
    for (auto it:keyChain) {
        msg << Cryptography::numberToString(it, false) << " ";
    }
    return msg.str();
}

void GM::onKeyExchangeResponseRecv(string msg) {
    Log->debug("onKeyExchangeResponseRecv/msg: {}", msg);
    istringstream stream(msg);
    vector<ZZ> gn_buffer;
    string tmp;
    while (stream >> tmp) {
        gn_buffer.push_back(Cryptography::stringToNumber(tmp, false));
    }
    keyChain = gn_buffer;

    //set groupKey
    groupKey = PowerMod(*keyChain.rbegin(), rsa_a, rsa_n);
    Log->info("Group key update: {}", Cryptography::numberToString(groupKey, false));

}

string GM::getBroadcastMsg() {
    //广播消息的格式是id gn id gn id gn...
    stringstream broadcast_buf;
    auto it_i = info.rbegin();
    if (keyChain.size() != 1 && keyChain.size() != info.size() + 1) {
        Log->critical("size don't match!!");
    }
    if (keyChain.size() == 1) {
        broadcast_buf << it_i->id << ' ';
        broadcast_buf << Cryptography::numberToString(
                PowerMod(this->g, rsa_a, rsa_n), false);
    } else {
        for (auto it = keyChain.begin(); it != keyChain.end() && it_i != info.rend(); ++it, ++it_i) {
            broadcast_buf << it_i->id << ' ';
            broadcast_buf << Cryptography::numberToString(
                    PowerMod(*it % n, rsa_a, rsa_n), false) << ' ';
        }
    }
    Log->debug("broadcast: {}", broadcast_buf.str());
    return broadcast_buf.str();
}