#include <stdio.h>
#include <iostream>
#include <time.h>
#include "sm2.h"
#include "sm3.h"
extern "C" {
#include "miracl.h"
#include "mirdef.h"
}

ECC Ecc256 = {
"FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFF",
"FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFC",
"28E9FA9E9D9F5E344D5A9E4BCF6509A7F39789F515AB8F92DDBCBD414D940E93",
"FFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF7203DF6B21C6052B53BBF40939D54123",
"32C4AE2C1F1981195F9904466A39C9948FE30BBFF2660BE1715A4589334C74C7",
"BC3736A2F4F6779C59BDCEE36B692153D0A9877CC62A474002DF32E52139F0A0"
};

void key_generate(ECC* ecc, unsigned char* private_key, unsigned char* pk_x, unsigned char *pk_y, unsigned char *Za){
	unsigned char Z[200] = { 0x00, 0x03, 's', 'm', '2' };	//ENTL||ID
	big p, a, b, x, y, n, k;
	epoint *g;
	miracl *mip = mirsys(300, 0);
	p = mirvar(0);
	a = mirvar(0);
	b = mirvar(0);
	x = mirvar(0);
	y = mirvar(0);
	n = mirvar(0);
	k = mirvar(0);
	mip->IOBASE = 16;
	cinstr(p, ecc->p);
	cinstr(a, ecc->a);
	cinstr(b, ecc->b);
	cinstr(n, ecc->n);
	cinstr(x, ecc->x);
	cinstr(y, ecc->y);
	ecurve_init(a, b, p, MR_PROJECTIVE);	//初始化椭圆曲线
	g = epoint_init();	//初始化点g
	epoint_set(x, y, 0, g);
	irand((unsigned int)time(NULL));
	bigrand(n, k);	//生成随机数k,作为私钥
	ecurve_mult(k, g, g);	//计算[k]G
	epoint_get(g, x, y);
	big_to_bytes(32, k, (char*)private_key, TRUE);
	big_to_bytes(32, x, (char*)pk_x, TRUE);
	big_to_bytes(32, y, (char*)pk_y, TRUE);

	//Za = H256(ENTLA ∥ IDA ∥ a ∥ b ∥ xG ∥yG ∥ xA ∥ yA)。
	memcpy(Z, ecc->a, 32);
	memcpy(Z + 32, ecc->b, 32);
	memcpy(Z + 64, ecc->x, 32);
	memcpy(Z + 96, ecc->y, 32);
	memcpy(Z + 128, pk_x, 32);
	memcpy(Z + 160, pk_y, 32);
	SM3_256(Z, 195, Za);

	mirkill(k);
	mirkill(p);
	mirkill(a);
	mirkill(b);
	mirkill(n);
	mirkill(x);
	mirkill(y);
	epoint_free(g);
	mirexit();
}

void sm2_sign(ECC* ecc, unsigned char *msg, int msg_len, unsigned char *Za, unsigned char* private_key, unsigned char *sign_r, unsigned char *sign_s) {
	unsigned char m_hash[32] = { 0 };
	unsigned char *m = (unsigned char*)malloc((msg_len + 32) * sizeof(unsigned char));
	memcpy(m , Za, 32);
	memcpy(m + 32, msg, msg_len);
	SM3_256(m, msg_len + 32, m_hash);
	big p, a, b, x, y, n, k, r, s, e, key, temp;
	epoint *g;
	miracl *mip = mirsys(300, 0);
	p = mirvar(0);
	a = mirvar(0);
	b = mirvar(0);
	x = mirvar(0);
	y = mirvar(0);
	n = mirvar(0);
	k = mirvar(0);
	r = mirvar(0);
	s = mirvar(0);
	e = mirvar(0);
	key = mirvar(0);
	temp = mirvar(0);
	mip->IOBASE = 16;
	cinstr(p, ecc->p);
	cinstr(a, ecc->a);
	cinstr(b, ecc->b);
	cinstr(x, ecc->x);
	cinstr(y, ecc->y);
	cinstr(n, ecc->n);
	bytes_to_big(32, (char*)private_key, key);
	bytes_to_big(32, (char*)m_hash, e);
	ecurve_init(a, b, p, MR_PROJECTIVE);	//初始化椭圆曲线
	g = epoint_init();	//初始化点g
	epoint_set(x, y, 0, g);

	while (1) {
		irand((unsigned int)time(NULL));
		bigrand(n, k);	//生成随机数k
		ecurve_mult(k, g, g);	//计算[k]G
		epoint_get(g, x, y);
		add(e, x, r);
		divide(r, n, n);	//r = (e + x) mod n
		add(r, k, temp);
		if (r->len == 0 || mr_compare(temp, n) == 0)
			continue;
		incr(key, 1, temp);	//key+1
		xgcd(temp, n, temp, temp, temp); 
		multiply(r, key, a);	//a = r*key
		divide(a, n, n);
		if (mr_compare(k, a) >= 0)
		{
			subtract(k, a, a);
		}
		else						//Attention!
		{
			subtract(n, a, a);
			add(k, a, a);
		}
		mad(temp, a, n, n, n, s);	//s = (temp * (k - r*key)) mod n
		if (s->len == 0)
			continue;
		big_to_bytes(32, r, (char*)sign_r, TRUE);
		big_to_bytes(32, s, (char*)sign_s, TRUE);
		break;
	}

	mirkill(e);
	mirkill(r);
	mirkill(s);
	mirkill(k);
	mirkill(p);
	mirkill(a);
	mirkill(b);
	mirkill(n);
	mirkill(x);
	mirkill(y);
	mirkill(key);
	mirkill(temp);
	epoint_free(g);
	mirexit();
}

int sm2_verify(ECC* ecc, unsigned char *msg, int msg_len, unsigned char *Za, unsigned char  *sign_r, unsigned char *sign_s, unsigned char *pk_x,unsigned char *pk_y){
	unsigned char m_hash[32] = { 0 };
	unsigned char *m = (unsigned char*)malloc((msg_len + 32) * sizeof(unsigned char));
	memcpy(m, Za, 32);
	memcpy(m + 32, msg, msg_len);
	SM3_256(m, msg_len + 32, m_hash);
	big p, a, b, x, y, n, k, r, s, e, pkx, pky, temp;
	epoint *g, *pa;
	miracl *mip = mirsys(300, 0);
	p = mirvar(0);
	a = mirvar(0);
	b = mirvar(0);
	x = mirvar(0);
	y = mirvar(0);
	n = mirvar(0);
	k = mirvar(0);
	r = mirvar(0);
	s = mirvar(0);
	e = mirvar(0);
	pkx = mirvar(0);
	pky = mirvar(0);
	temp = mirvar(0);
	mip->IOBASE = 16;
	cinstr(p, ecc->p);
	cinstr(a, ecc->a);
	cinstr(b, ecc->b);
	cinstr(x, ecc->x);
	cinstr(y, ecc->y);
	cinstr(n, ecc->n);
	bytes_to_big(32, (char*)m_hash, e);
	bytes_to_big(32, (char*)sign_r, r);
	bytes_to_big(32, (char*)sign_s, s);
	bytes_to_big(32, (char*)pk_x, pkx);
	bytes_to_big(32, (char*)pk_y, pky);
	ecurve_init(a, b, p, MR_PROJECTIVE);
	g = epoint_init();
	pa = epoint_init();
	epoint_set(x, y, 0, g);
	epoint_set(pkx, pky, 0, pa);
	if (r->len == 0 || mr_compare(r, n) >= 0)
		return 0;
	if (s->len == 0 || mr_compare(s, n) >= 0)
		return 0;
	add(r, s, temp);
	divide(temp, n, n);		//t = (r + s) mod n
	if (temp->len == 0)
		return 0;
	ecurve_mult2(s, g, temp, pa, g);	//[s]G + [t]Pa
	epoint_get(g, x, y);
	add(e, x, x);
	divide(x, n, n);	// (e + x) mod n
	if (mr_compare(x, r) != 0)
		return 0;
	return 1;
}