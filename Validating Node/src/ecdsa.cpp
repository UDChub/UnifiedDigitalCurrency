/*
	Copyright (c) 2016-2017, Unified Digital Currency

	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice, this
	list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
	this list of conditions and the following disclaimer in the documentation
	and/or other materials provided with the distribution.

	3. Neither the name of the copyright holder nor the names of its contributors
	may be used to endorse or promote products derived from this software without
	specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**
 * @file ecdsa.cpp
 * @author Rafael Afonso Rodrigues <founder@udc.world>
 * @date 2016
 * UDC Validating Node.
 */

#include <string>
#include <sstream>
#include <iostream>

#include "includes/cryptopp/eccrypto.h"
#include "includes/cryptopp/filters.h"
#include "includes/cryptopp/files.h"
#include "includes/cryptopp/hex.h"
#include "includes/cryptopp/integer.h"
#include "includes/cryptopp/nbtheory.h"
#include "includes/cryptopp/oids.h"
#include "includes/cryptopp/osrng.h"
#include "includes/cryptopp/ripemd.h"
#include "includes/cryptopp/sha.h"

#include "configurations.h"
#include "ecdsa.h"
#include "globals.h"
#include "util.h"


bool Crypto::GenerateKeys() {
	CryptoPP::ECDSA<CryptoPP::ECP,CryptoPP::SHA256>::PublicKey publicKey;
	CryptoPP::AutoSeededRandomPool prng;

	PRIVATE_KEY.Initialize(prng, CryptoPP::ASN1::secp384r1());
	if(!PRIVATE_KEY.Validate(prng, 3)) return false;

	PRIVATE_KEY.MakePublicKey(publicKey);
	if(!publicKey.Validate(prng, 3)) return false;

	CryptoPP::FileSink fsPriv(_ECDSA_PRIVATE_KEY.c_str(), true);
	PRIVATE_KEY.Save(fsPriv);
	std::cout << "This node's Private ECDSA Key has been successfully created and saved to " << _ECDSA_PRIVATE_KEY << std::endl;
	CryptoPP::FileSink fsPub(_ECDSA_PUBLIC_KEY.c_str(), true);
	publicKey.Save(fsPub);
	std::cout << "This node's Public ECDSA Key has been successfully created and saved to " << _ECDSA_PUBLIC_KEY << std::endl;

	return true;
}

 bool Crypto::LoadPrivateKey() {
	CryptoPP::FileSource fs(_ECDSA_PRIVATE_KEY.c_str(), true);
	PRIVATE_KEY.Load(fs);

	CryptoPP::AutoSeededRandomPool prng;	
	if(PRIVATE_KEY.Validate(prng, 3)) return true;
	return false;
}

bool Crypto::GetPublicKey(std::string &publicKey) {
	CryptoPP::ECDSA<CryptoPP::ECP,CryptoPP::SHA256>::PublicKey pub;

	keyMutex.lock();
	CryptoPP::FileSource fs(_ECDSA_PUBLIC_KEY.c_str(), true);
	pub.Load(fs);
	keyMutex.unlock();

	CryptoPP::AutoSeededRandomPool prng;	
	if(pub.Validate(prng, 3)) {

		const CryptoPP::ECP::Point& p = pub.GetPublicElement();
		const CryptoPP::Integer &x = p.x;
		const CryptoPP::Integer &y = p.y;

		std::stringstream ss;
		ss << std::hex << std::uppercase << x << y;

		std::string hexKey = ss.str();
		//Remove "h" indicators
		hexKey.replace((hexKey.length()/2)-1,1,"");
		hexKey.replace(hexKey.length()-1,1,"");
		//Add key prefix
		hexKey = "04" + hexKey;

		//Convert key's bytes to base64
		publicKey = Util::hex_to_string(hexKey);
		publicKey = Util::string_to_base64(publicKey);
		return true;
	}

	return false;
}

std::string Crypto::Sign(std::string message, bool file /*=false*/, int encoding /*=ENCODING_BASE64*/) {	
	keyMutex.lock();
	CryptoPP::ECDSA<CryptoPP::ECP,CryptoPP::SHA256>::Signer signer(PRIVATE_KEY);
	keyMutex.unlock();

	CryptoPP::AutoSeededRandomPool prng;
	std::string signature;

	if(file) {
		//Hash file first
		CryptoPP::RIPEMD160 ripemd;
		CryptoPP::FileSource fs(message.c_str(), true, new CryptoPP::SignerFilter(prng, signer, new CryptoPP::StringSink(message)));
	}
	CryptoPP::StringSource ss(message, true, new CryptoPP::SignerFilter(prng, signer, new CryptoPP::StringSink(signature)));

	switch(encoding) {
		case ENCODING_HEX:
			return Util::string_to_hex(signature);

		case ENCODING_BASE64:
			return Util::string_to_base64(signature);

		default:
			return signature;
	}	
}
	
bool Crypto::Verify(std::string message, std::string signature, std::string publicKey, bool file /*=false*/, int encoding /*=ENCODING_BASE64*/) {
	bool result = false;

	//Decode point
	std::string hexKey = Util::string_to_hex(publicKey);
	CryptoPP::HexDecoder decoder;
	decoder.Put((byte*) hexKey.data(), hexKey.size());
	decoder.MessageEnd();
	size_t len = decoder.MaxRetrievable();
	if(len%2) return false;

	//Create point
	CryptoPP::ECP::Point p;
	p.identity = false;
	p.x.Decode(decoder, len/2);
	p.y.Decode(decoder, len/2);

	//Initialize public key
	CryptoPP::ECDSA<CryptoPP::ECP,CryptoPP::SHA256>::PublicKey pub;
	switch(len) {
		//EC SECP256R1
		case 64:
			pub.Initialize(CryptoPP::ASN1::secp256r1(), p);
			break;

		//EC SECP384R1
		case 96:
			pub.Initialize(CryptoPP::ASN1::secp384r1(), p);
			break;

		//EC SECP521R1
		case 132:
			pub.Initialize(CryptoPP::ASN1::secp521r1(), p);
			break;

		//Unsupported public key curve
		default: return false;
	}

	CryptoPP::ECDSA<CryptoPP::ECP,CryptoPP::SHA256>::Verifier verifier(pub);

	std::string decodedSignature;
	switch(encoding) {
		case ENCODING_HEX:
			decodedSignature = Util::hex_to_string(signature);
			break;

		case ENCODING_BASE64:
			decodedSignature = Util::base64_to_string(signature);
			break;

		default:
			decodedSignature = signature;
			break;
	}

	try{
		if(file) {
			//Hash file first
			std::string hash;
			CryptoPP::RIPEMD160 ripemd;
			CryptoPP::FileSource fs(message.c_str(), true, new CryptoPP::HashFilter(ripemd, new CryptoPP::HexEncoder(new CryptoPP::StringSink(hash))));
			message = hash;
		}

		CryptoPP::StringSource ss(message+decodedSignature, true, 
			new CryptoPP::SignatureVerificationFilter(
				verifier, 
				new CryptoPP::ArraySink((byte*)&result, sizeof(result)),
				CryptoPP::SignatureVerificationFilter::THROW_EXCEPTION | CryptoPP::SignatureVerificationFilter::PUT_RESULT
		));
	}
	catch(CryptoPP::Exception& e) return false;
	return result;
}

bool Crypto::IsNIST(std::string key) {
	//Decode point
	std::string hexKey = Util::string_to_hex(key);
	CryptoPP::HexDecoder decoder;
	decoder.Put((byte*) hexKey.data(), hexKey.size());
	decoder.MessageEnd();
	size_t len = decoder.MaxRetrievable();
	if((len/2) % 2) return false;

	//Create point
	CryptoPP::ECP::Point p;
	p.identity = false;
	p.x.Decode(decoder, len/2);
	p.y.Decode(decoder, len/2);

	//Initialize public key
	CryptoPP::ECDSA<CryptoPP::ECP,CryptoPP::SHA256>::PublicKey pub;
	switch(key.length()) {
		//EC SECP256R1
		case 64:
			pub.Initialize(CryptoPP::ASN1::secp256r1(), p);
			break;

		//EC SECP384R1
		case 96:
			pub.Initialize(CryptoPP::ASN1::secp384r1(), p);
			break;

		//EC SECP521R1
		case 132:
			pub.Initialize(CryptoPP::ASN1::secp521r1(), p);
			break;

		//Unsupported public key curve
		default: return false;
	}

	CryptoPP::AutoSeededRandomPool prng;
	return pub.Validate(prng, 3);
}

//Finds y coordinate using x coordinate from the compressed public key
//both points are in hex string
bool Crypto::SolveYPoint(std::string xPoint, std::string &yPoint) {
	CryptoPP::ECDSA<CryptoPP::ECP,CryptoPP::SHA256>::PublicKey pub;

	//Initialize public key with corresponding curve
	uint length = xPoint.length();
	switch(length) {
		case 64:
			pub.AccessGroupParameters().Initialize(CryptoPP::ASN1::secp256r1());
			break;
		case 96:
			pub.AccessGroupParameters().Initialize(CryptoPP::ASN1::secp384r1());
			break;
		case 132:
			pub.AccessGroupParameters().Initialize(CryptoPP::ASN1::secp521r1());
			break;
		default: return false;
	}

	//Retrieve curve parameters
	CryptoPP::Integer p = pub.GetGroupParameters().GetCurve().FieldSize();
	CryptoPP::Integer a = pub.GetGroupParameters().GetCurve().GetA();
	CryptoPP::Integer b = pub.GetGroupParameters().GetCurve().GetB();

	//set x point
	xPoint = "0x"+xPoint;
	CryptoPP::Integer x(xPoint.c_str());

	//put values into the curve's base field
	x %= p;
	a %= p;
	b %= p;

	//solve y point
	CryptoPP::Integer y = a_exp_b_mod_c(x, 3, p);
	y += a * x + b;
	y = CryptoPP::ModularSquareRoot(y % p, p);

	//check if it was found
	if(y == 0) return false;

	//Extract y point
	std::stringstream ss;
	ss << std::hex << std::uppercase << y;
	yPoint = ss.str();

	//remove "h" suffix
	yPoint = yPoint.substr(0, yPoint.length()-1);

	//Ensure it is valid
	switch(yPoint.length()-length) {
		case -1:
			yPoint = "00"+yPoint;
		case 0:
			return true;

		default: return false;
	}
}