#include <string>
const std::string KERNEL_PROG=R"(
typedef unsigned int sph_u32;
#define SPH_C32(x) ((sph_u32)(x ## U))
#define SPH_T32(x) (as_uint(x))
#define SPH_ROTL32(x, n) rotate(as_uint(x), as_uint(n))
#define SPH_ROTR32(x, n) SPH_ROTL32(x, (32-(n)))
#define SPH_C64(x) ((sph_u64)(x ## UL))
#define SPH_T64(x) (as_ulong(x))
#define SPH_ROTL64(x, n) rotate(as_ulong(x), (n) & 0xFFFFFFFFFFFFFFFFUL)
#define SPH_ROTR64(x, n) SPH_ROTL64(x, (64-(n)))
#define sM 16
#define C32 SPH_C32
#define T32 SPH_T32
#define O1 13
#define O2 9
#define O3 6
#define INPUT_BLOCK_ADD do { \
	B0=T32(B0+M0); \
	B1=T32(B1+M1); \
	B2=T32(B2+M2); \
	B3=T32(B3+M3); \
	B4=T32(B4+M4); \
	B5=T32(B5+M5); \
	B6=T32(B6+M6); \
	B7=T32(B7+M7); \
	B8=T32(B8+M8); \
	B9=T32(B9+M9); \
	BA=T32(BA+MA); \
	BB=T32(BB+MB); \
	BC=T32(BC+MC); \
	BD=T32(BD+MD); \
	BE=T32(BE+ME); \
	BF=T32(BF+MF); \
} while (0)
#define INPUT_BLOCK_SUB do { \
	C0=T32(C0-M0); \
	C1=T32(C1-M1); \
	C2=T32(C2-M2); \
	C3=T32(C3-M3); \
	C4=T32(C4-M4); \
	C5=T32(C5-M5); \
	C6=T32(C6-M6); \
	C7=T32(C7-M7); \
	C8=T32(C8-M8); \
	C9=T32(C9-M9); \
	CA=T32(CA-MA); \
	CB=T32(CB-MB); \
	CC=T32(CC-MC); \
	CD=T32(CD-MD); \
	CE=T32(CE-ME); \
	CF=T32(CF-MF); \
} while (0)

#define XOR_W do { \
	A00^=Wlow; \
	A01^=Whigh; \
} while (0)

#define SWAP(v1, v2) do { \
	sph_u32 tmp=(v1); \
	(v1)=(v2); \
	(v2)=tmp; \
} while (0)

#define SWAP_BC do { \
	SWAP(B0,C0); \
	SWAP(B1,C1); \
	SWAP(B2,C2); \
	SWAP(B3,C3); \
	SWAP(B4,C4); \
	SWAP(B5,C5); \
	SWAP(B6,C6); \
	SWAP(B7,C7); \
	SWAP(B8,C8); \
	SWAP(B9,C9); \
	SWAP(BA,CA); \
	SWAP(BB,CB); \
	SWAP(BC,CC); \
	SWAP(BD,CD); \
	SWAP(BE,CE); \
	SWAP(BF,CF); \
} while (0)

#define PERM_ELT(xa0, xa1, xb0, xb1, xb2, xb3, xc, xm) do { \
	xa0=T32((xa0 \
	^ (((xa1<<15)|(xa1>>17)) * 5U) \
	^ xc) * 3U) \
	^ xb1 ^ (xb2 & ~xb3) ^ xm; \
	xb0=T32(~(((xb0<<1)|(xb0>>31)) ^ xa0)); \
} while (0)

#define PERM_STEP_0 do { \
	PERM_ELT(A00,A0B,B0,BD,B9,B6,C8,M0); \
	PERM_ELT(A01,A00,B1,BE,BA,B7,C7,M1); \
	PERM_ELT(A02,A01,B2,BF,BB,B8,C6,M2); \
	PERM_ELT(A03,A02,B3,B0,BC,B9,C5,M3); \
	PERM_ELT(A04,A03,B4,B1,BD,BA,C4,M4); \
	PERM_ELT(A05,A04,B5,B2,BE,BB,C3,M5); \
	PERM_ELT(A06,A05,B6,B3,BF,BC,C2,M6); \
	PERM_ELT(A07,A06,B7,B4,B0,BD,C1,M7); \
	PERM_ELT(A08,A07,B8,B5,B1,BE,C0,M8); \
	PERM_ELT(A09,A08,B9,B6,B2,BF,CF,M9); \
	PERM_ELT(A0A,A09,BA,B7,B3,B0,CE,MA); \
	PERM_ELT(A0B,A0A,BB,B8,B4,B1,CD,MB); \
	PERM_ELT(A00,A0B,BC,B9,B5,B2,CC,MC); \
	PERM_ELT(A01,A00,BD,BA,B6,B3,CB,MD); \
	PERM_ELT(A02,A01,BE,BB,B7,B4,CA,ME); \
	PERM_ELT(A03,A02,BF,BC,B8,B5,C9,MF); \
} while (0)

#define PERM_STEP_1 do { \
	PERM_ELT(A04,A03,B0,BD,B9,B6,C8,M0); \
	PERM_ELT(A05,A04,B1,BE,BA,B7,C7,M1); \
	PERM_ELT(A06,A05,B2,BF,BB,B8,C6,M2); \
	PERM_ELT(A07,A06,B3,B0,BC,B9,C5,M3); \
	PERM_ELT(A08,A07,B4,B1,BD,BA,C4,M4); \
	PERM_ELT(A09,A08,B5,B2,BE,BB,C3,M5); \
	PERM_ELT(A0A,A09,B6,B3,BF,BC,C2,M6); \
	PERM_ELT(A0B,A0A,B7,B4,B0,BD,C1,M7); \
	PERM_ELT(A00,A0B,B8,B5,B1,BE,C0,M8); \
	PERM_ELT(A01,A00,B9,B6,B2,BF,CF,M9); \
	PERM_ELT(A02,A01,BA,B7,B3,B0,CE,MA); \
	PERM_ELT(A03,A02,BB,B8,B4,B1,CD,MB); \
	PERM_ELT(A04,A03,BC,B9,B5,B2,CC,MC); \
	PERM_ELT(A05,A04,BD,BA,B6,B3,CB,MD); \
	PERM_ELT(A06,A05,BE,BB,B7,B4,CA,ME); \
	PERM_ELT(A07,A06,BF,BC,B8,B5,C9,MF); \
} while (0)

#define PERM_STEP_2 do { \
	PERM_ELT(A08,A07,B0,BD,B9,B6,C8,M0); \
	PERM_ELT(A09,A08,B1,BE,BA,B7,C7,M1); \
	PERM_ELT(A0A,A09,B2,BF,BB,B8,C6,M2); \
	PERM_ELT(A0B,A0A,B3,B0,BC,B9,C5,M3); \
	PERM_ELT(A00,A0B,B4,B1,BD,BA,C4,M4); \
	PERM_ELT(A01,A00,B5,B2,BE,BB,C3,M5); \
	PERM_ELT(A02,A01,B6,B3,BF,BC,C2,M6); \
	PERM_ELT(A03,A02,B7,B4,B0,BD,C1,M7); \
	PERM_ELT(A04,A03,B8,B5,B1,BE,C0,M8); \
	PERM_ELT(A05,A04,B9,B6,B2,BF,CF,M9); \
	PERM_ELT(A06,A05,BA,B7,B3,B0,CE,MA); \
	PERM_ELT(A07,A06,BB,B8,B4,B1,CD,MB); \
	PERM_ELT(A08,A07,BC,B9,B5,B2,CC,MC); \
	PERM_ELT(A09,A08,BD,BA,B6,B3,CB,MD); \
	PERM_ELT(A0A,A09,BE,BB,B7,B4,CA,ME); \
	PERM_ELT(A0B,A0A,BF,BC,B8,B5,C9,MF); \
} while (0)

#define APPLY_P do { \
	B0=T32(B0<<17)|(B0>>15); \
	B1=T32(B1<<17)|(B1>>15); \
	B2=T32(B2<<17)|(B2>>15); \
	B3=T32(B3<<17)|(B3>>15); \
	B4=T32(B4<<17)|(B4>>15); \
	B5=T32(B5<<17)|(B5>>15); \
	B6=T32(B6<<17)|(B6>>15); \
	B7=T32(B7<<17)|(B7>>15); \
	B8=T32(B8<<17)|(B8>>15); \
	B9=T32(B9<<17)|(B9>>15); \
	BA=T32(BA<<17)|(BA>>15); \
	BB=T32(BB<<17)|(BB>>15); \
	BC=T32(BC<<17)|(BC>>15); \
	BD=T32(BD<<17)|(BD>>15); \
	BE=T32(BE<<17)|(BE>>15); \
	BF=T32(BF<<17)|(BF>>15); \
	PERM_STEP_0; \
	PERM_STEP_1; \
	PERM_STEP_2; \
	A0B=T32(A0B+C6); \
	A0A=T32(A0A+C5); \
	A09=T32(A09+C4); \
	A08=T32(A08+C3); \
	A07=T32(A07+C2); \
	A06=T32(A06+C1); \
	A05=T32(A05+C0); \
	A04=T32(A04+CF); \
	A03=T32(A03+CE); \
	A02=T32(A02+CD); \
	A01=T32(A01+CC); \
	A00=T32(A00+CB); \
	A0B=T32(A0B+CA); \
	A0A=T32(A0A+C9); \
	A09=T32(A09+C8); \
	A08=T32(A08+C7); \
	A07=T32(A07+C6); \
	A06=T32(A06+C5); \
	A05=T32(A05+C4); \
	A04=T32(A04+C3); \
	A03=T32(A03+C2); \
	A02=T32(A02+C1); \
	A01=T32(A01+C0); \
	A00=T32(A00+CF); \
	A0B=T32(A0B+CE); \
	A0A=T32(A0A+CD); \
	A09=T32(A09+CC); \
	A08=T32(A08+CB); \
	A07=T32(A07+CA); \
	A06=T32(A06+C9); \
	A05=T32(A05+C8); \
	A04=T32(A04+C7); \
	A03=T32(A03+C6); \
	A02=T32(A02+C5); \
	A01=T32(A01+C4); \
	A00=T32(A00+C3); \
} while (0)

#define INCR_W do { \
	if ((Wlow=T32(Wlow+1))==0) \
	Whigh=T32(Whigh+1); \
} while (0)

__constant static const sph_u32 A_init_192[]={
C32(0xFD749ED4),C32(0xB798E530),C32(0x33904B6F),C32(0x46BDA85E),
C32(0x076934B4),C32(0x454B4058),C32(0x77F74527),C32(0xFB4CF465),
C32(0x62931DA9),C32(0xE778C8DB),C32(0x22B3998E),C32(0xAC15CFB9)
};

__constant static const sph_u32 B_init_192[]={
C32(0x58BCBAC4),C32(0xEC47A08E),C32(0xAEE933B2),C32(0xDFCBC824),
C32(0xA7944804),C32(0xBF65BDB0),C32(0x5A9D4502),C32(0x59979AF7),
C32(0xC5CEA54E),C32(0x4B6B8150),C32(0x16E71909),C32(0x7D632319),
C32(0x930573A0),C32(0xF34C63D1),C32(0xCAF914B4),C32(0xFDD6612C)
};

__constant static const sph_u32 C_init_192[]={
C32(0x61550878),C32(0x89EF2B75),C32(0xA1660C46),C32(0x7EF3855B),
C32(0x7297B58C),C32(0x1BC67793),C32(0x7FB1C723),C32(0xB66FC640),
C32(0x1A48B71C),C32(0xF0976D17),C32(0x088CE80A),C32(0xA454EDF3),
C32(0x1C096BF4),C32(0xAC76224B),C32(0x5215781C),C32(0xCD5D2669)
};

__constant static const sph_u32 A_init_224[]={
C32(0xA5201467),C32(0xA9B8D94A),C32(0xD4CED997),C32(0x68379D7B),
C32(0xA7FC73BA),C32(0xF1A2546B),C32(0x606782BF),C32(0xE0BCFD0F),
C32(0x2F25374E),C32(0x069A149F),C32(0x5E2DFF25),C32(0xFAECF061)
};

__constant static const sph_u32 B_init_224[]={
C32(0xEC9905D8),C32(0xF21850CF),C32(0xC0A746C8),C32(0x21DAD498),
C32(0x35156EEB),C32(0x088C97F2),C32(0x26303E40),C32(0x8A2D4FB5),
C32(0xFEEE44B6),C32(0x8A1E9573),C32(0x7B81111A),C32(0xCBC139F0),
C32(0xA3513861),C32(0x1D2C362E),C32(0x918C580E),C32(0xB58E1B9C)
};

__constant static const sph_u32 C_init_224[]={
C32(0xE4B573A1),C32(0x4C1A0880),C32(0x1E907C51),C32(0x04807EFD),
C32(0x3AD8CDE5),C32(0x16B21302),C32(0x02512C53),C32(0x2204CB18),
C32(0x99405F2D),C32(0xE5B648A1),C32(0x70AB1D43),C32(0xA10C25C2),
C32(0x16F1AC05),C32(0x38BBEB56),C32(0x9B01DC60),C32(0xB1096D83)
};

__constant static const sph_u32 A_init_256[]={
C32(0x52F84552),C32(0xE54B7999),C32(0x2D8EE3EC),C32(0xB9645191),
C32(0xE0078B86),C32(0xBB7C44C9),C32(0xD2B5C1CA),C32(0xB0D2EB8C),
C32(0x14CE5A45),C32(0x22AF50DC),C32(0xEFFDBC6B),C32(0xEB21B74A)
};

__constant static const sph_u32 B_init_256[]={
C32(0xB555C6EE),C32(0x3E710596),C32(0xA72A652F),C32(0x9301515F),
C32(0xDA28C1FA),C32(0x696FD868),C32(0x9CB6BF72),C32(0x0AFE4002),
C32(0xA6E03615),C32(0x5138C1D4),C32(0xBE216306),C32(0xB38B8890),
C32(0x3EA8B96B),C32(0x3299ACE4),C32(0x30924DD4),C32(0x55CB34A5)
};

__constant static const sph_u32 C_init_256[]={
C32(0xB405F031),C32(0xC4233EBA),C32(0xB3733979),C32(0xC0DD9D55),
C32(0xC51C28AE),C32(0xA327B8E1),C32(0x56C56167),C32(0xED614433),
C32(0x88B59D60),C32(0x60E2CEBA),C32(0x758B4B8B),C32(0x83E82A7F),
C32(0xBC968828),C32(0xE6E00BF7),C32(0xBA839E55),C32(0x9B491C60)
};

__constant static const sph_u32 A_init_384[]={
C32(0xC8FCA331),C32(0xE55C504E),C32(0x003EBF26),C32(0xBB6B8D83),
C32(0x7B0448C1),C32(0x41B82789),C32(0x0A7C9601),C32(0x8D659CFF),
C32(0xB6E2673E),C32(0xCA54C77B),C32(0x1460FD7E),C32(0x3FCB8F2D)
};

__constant static const sph_u32 B_init_384[]={
C32(0x527291FC),C32(0x2A16455F),C32(0x78E627E5),C32(0x944F169F),
C32(0x1CA6F016),C32(0xA854EA25),C32(0x8DB98ABE),C32(0xF2C62641),
C32(0x30117DCB),C32(0xCF5C4309),C32(0x93711A25),C32(0xF9F671B8),
C32(0xB01D2116),C32(0x333F4B89),C32(0xB285D165),C32(0x86829B36)
};

__constant static const sph_u32 C_init_384[]={
C32(0xF764B11A),C32(0x76172146),C32(0xCEF6934D),C32(0xC6D28399),
C32(0xFE095F61),C32(0x5E6018B4),C32(0x5048ECF5),C32(0x51353261),
C32(0x6E6E36DC),C32(0x63130DAD),C32(0xA9C69BD6),C32(0x1E90EA0C),
C32(0x7C35073B),C32(0x28D95E6D),C32(0xAA340E0D),C32(0xCB3DEE70)
};

__constant static const sph_u32 A_init_512[]={
C32(0x20728DFD),C32(0x46C0BD53),C32(0xE782B699),C32(0x55304632),
C32(0x71B4EF90),C32(0x0EA9E82C),C32(0xDBB930F1),C32(0xFAD06B8B),
C32(0xBE0CAE40),C32(0x8BD14410),C32(0x76D2ADAC),C32(0x28ACAB7F)
};

__constant static const sph_u32 B_init_512[]={
C32(0xC1099CB7),C32(0x07B385F3),C32(0xE7442C26),C32(0xCC8AD640),
C32(0xEB6F56C7),C32(0x1EA81AA9),C32(0x73B9D314),C32(0x1DE85D08),
C32(0x48910A5A),C32(0x893B22DB),C32(0xC5A0DF44),C32(0xBBC4324E),
C32(0x72D2F240),C32(0x75941D99),C32(0x6D8BDE82),C32(0xA1A7502B)
};

__constant static const sph_u32 C_init_512[]={
C32(0xD9BF68D1),C32(0x58BAD750),C32(0x56028CB2),C32(0x8134F359),
C32(0xB5D469D8),C32(0x941A8CC2),C32(0x418B2A6E),C32(0x04052780),
C32(0x7F07D787),C32(0x5194358F),C32(0x3C60D665),C32(0xBE97D79A),
C32(0x950C3434),C32(0xAED9A06D),C32(0x2537DC8D),C32(0x7CDB5969)
};

#define NONCES_VECTOR 16
#define NONCES_VECTOR_MASK 15
#define NONCES_VECTOR_ALIGN (~15)
#define MESSAGE_CAP 64
#define NUM_HASHES 8192
#define HASH_SIZE_WORDS 8
#define NONCE_SIZE_WORDS HASH_SIZE_WORDS * NUM_HASHES
#define EndianSwap(n) (rotate(n & 0x00FF00FF, 24UL)|(rotate(n, 8UL) & 0x00FF00FF))
#define EndianSwap64(n) bitselect( \
bitselect(rotate(n, 24UL), \
rotate(n, 8UL), 0x000000FF000000FFUL), \
bitselect(rotate(n, 56UL), \
rotate(n, 40UL), 0x00FF000000FF0000UL), \
0xFFFF0000FFFF0000UL)

#define Address(nonce,hash,word) \
((nonce) & NONCES_VECTOR_ALIGN) * NONCE_SIZE_WORDS \
+ (hash) * NONCES_VECTOR * HASH_SIZE_WORDS \
+ (word) * NONCES_VECTOR \
+ ((nonce) & NONCES_VECTOR_MASK)

__kernel void plotting(__global unsigned char* buffer, unsigned long startnonce, unsigned long numeric_id_be, int start, int end, unsigned long nonces) {
 int gid=get_global_id(0);
 if (gid >= nonces)
 return;
 int num;
 sph_u32 B8,B9,BA,BB,BC,BD,BE,BF;
 unsigned long nonce_be=EndianSwap64(startnonce+gid);
 for (int hash=start; hash > end; hash -= 1) {
  num=(NUM_HASHES-hash)>>1;
  if (hash != 0) {
   num=(num > MESSAGE_CAP) ? MESSAGE_CAP : num;
  }
  sph_u32
  A00=A_init_256[0],A01=A_init_256[1],A02=A_init_256[2],A03=A_init_256[3],
  A04=A_init_256[4],A05=A_init_256[5],A06=A_init_256[6],A07=A_init_256[7],
  A08=A_init_256[8],A09=A_init_256[9],A0A=A_init_256[10],A0B=A_init_256[11];
  sph_u32
  B0=B_init_256[0],B1=B_init_256[1],B2=B_init_256[2],B3=B_init_256[3],
  B4=B_init_256[4],B5=B_init_256[5],B6=B_init_256[6],B7=B_init_256[7];
  B8=B_init_256[8]; B9=B_init_256[9]; BA=B_init_256[10]; BB=B_init_256[11];
  BC=B_init_256[12]; BD=B_init_256[13]; BE=B_init_256[14]; BF=B_init_256[15];
  sph_u32
  C0=C_init_256[0],C1=C_init_256[1],C2=C_init_256[2],C3=C_init_256[3],
  C4=C_init_256[4],C5=C_init_256[5],C6=C_init_256[6],C7=C_init_256[7],
  C8=C_init_256[8],C9=C_init_256[9],CA=C_init_256[10],CB=C_init_256[11],
  CC=C_init_256[12],CD=C_init_256[13],CE=C_init_256[14],CF=C_init_256[15];
  sph_u32 M0,M1,M2,M3,M4,M5,M6,M7,M8,M9,MA,MB,MC,MD,ME,MF;
  sph_u32 Wlow=1, Whigh=0;
  for (int i=0; i < 2 * num; i+=2){
   M0=((__global unsigned int*)buffer)[Address(gid, hash+i, 0)];
   M1=((__global unsigned int*)buffer)[Address(gid, hash+i, 1)];
   M2=((__global unsigned int*)buffer)[Address(gid, hash+i, 2)];
   M3=((__global unsigned int*)buffer)[Address(gid, hash+i, 3)];
   M4=((__global unsigned int*)buffer)[Address(gid, hash+i, 4)];
   M5=((__global unsigned int*)buffer)[Address(gid, hash+i, 5)];
   M6=((__global unsigned int*)buffer)[Address(gid, hash+i, 6)];
   M7=((__global unsigned int*)buffer)[Address(gid, hash+i, 7)];
   M8=((__global unsigned int*)buffer)[Address(gid, hash+i+1, 0)];
   M9=((__global unsigned int*)buffer)[Address(gid, hash+i+1, 1)];
   MA=((__global unsigned int*)buffer)[Address(gid, hash+i+1, 2)];
   MB=((__global unsigned int*)buffer)[Address(gid, hash+i+1, 3)];
   MC=((__global unsigned int*)buffer)[Address(gid, hash+i+1, 4)];
   MD=((__global unsigned int*)buffer)[Address(gid, hash+i+1, 5)];
   ME=((__global unsigned int*)buffer)[Address(gid, hash+i+1, 6)];
   MF=((__global unsigned int*)buffer)[Address(gid, hash+i+1, 7)];

   INPUT_BLOCK_ADD;
   XOR_W;
   APPLY_P;
   INPUT_BLOCK_SUB;
   SWAP_BC;
   INCR_W;
  }
  if (num==MESSAGE_CAP) {
   M0=0x80;
   M1=M2=M3=M4=M5=M6=M7=M8=M9=MA=MB=MC=MD=ME=MF=0;
  }
  else if((hash & 1)==0) {
   M0=((unsigned int*)&numeric_id_be)[0];
   M1=((unsigned int*)&numeric_id_be)[1];
   M2=((unsigned int*)&nonce_be)[0];
   M3=((unsigned int*)&nonce_be)[1];
   M4=0x80;
   M5=M6=M7=M8=M9=MA=MB=MC=MD=ME=MF=0;
  }
  else if((hash & 1)==1) {
   M0=((__global unsigned int*)buffer)[Address(gid, NUM_HASHES-1, 0)];
   M1=((__global unsigned int*)buffer)[Address(gid, NUM_HASHES-1, 1)];
   M2=((__global unsigned int*)buffer)[Address(gid, NUM_HASHES-1, 2)];
   M3=((__global unsigned int*)buffer)[Address(gid, NUM_HASHES-1, 3)];
   M4=((__global unsigned int*)buffer)[Address(gid, NUM_HASHES-1, 4)];
   M5=((__global unsigned int*)buffer)[Address(gid, NUM_HASHES-1, 5)];
   M6=((__global unsigned int*)buffer)[Address(gid, NUM_HASHES-1, 6)];
   M7=((__global unsigned int*)buffer)[Address(gid, NUM_HASHES-1, 7)];
   M8=((unsigned int*)&numeric_id_be)[0];
   M9=((unsigned int*)&numeric_id_be)[1];
   MA=((unsigned int*)&nonce_be)[0];
   MB=((unsigned int*)&nonce_be)[1];
   MC=0x80;
   MD=ME=MF=0;
  }

  INPUT_BLOCK_ADD;
  XOR_W;
  APPLY_P;
  for (int i=0; i < 3; i ++) {
   SWAP_BC;
   XOR_W;
   APPLY_P;
  }

  if (hash > 0){
   ((__global unsigned int*)buffer)[Address(gid, hash-1, 0)]=B8;
   ((__global unsigned int*)buffer)[Address(gid, hash-1, 1)]=B9;
   ((__global unsigned int*)buffer)[Address(gid, hash-1, 2)]=BA;
   ((__global unsigned int*)buffer)[Address(gid, hash-1, 3)]=BB;
   ((__global unsigned int*)buffer)[Address(gid, hash-1, 4)]=BC;
   ((__global unsigned int*)buffer)[Address(gid, hash-1, 5)]=BD;
   ((__global unsigned int*)buffer)[Address(gid, hash-1, 6)]=BE;
   ((__global unsigned int*)buffer)[Address(gid, hash-1, 7)]=BF;
  }
 }
 if(end==-1){
  for (size_t i=0; i < NUM_HASHES; i++){
   ((__global unsigned int*)buffer)[Address(gid, i, 0)]^=B8;
   ((__global unsigned int*)buffer)[Address(gid, i, 1)]^=B9;
   ((__global unsigned int*)buffer)[Address(gid, i, 2)]^=BA;
   ((__global unsigned int*)buffer)[Address(gid, i, 3)]^=BB;
   ((__global unsigned int*)buffer)[Address(gid, i, 4)]^=BC;
   ((__global unsigned int*)buffer)[Address(gid, i, 5)]^=BD;
   ((__global unsigned int*)buffer)[Address(gid, i, 6)]^=BE;
   ((__global unsigned int*)buffer)[Address(gid, i, 7)]^=BF;
  }
 }
}
)";