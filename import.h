#pragma once

struct shared_ptr {
	void *obj;
	void *ctrl_blk;
};

union std_string {
	struct {
		uint8_t mark;
		char str[0];
	};
	struct {
		size_t cap;
		size_t size;
		const char *data;
	};
};

struct std_vector {
	void *begin;
	void *end;
	void *end_capacity;
};

static inline union std_string new_std_string(const char *s) {
	union std_string str = {
		.cap = 1,
		.size = strlen(s),
		.data = s,
	};
	return str;
}

static inline struct std_vector new_std_vector(void *begin) {
    struct std_vector vector = {
        .begin = begin,
        .end = begin + 1,
    };
    vector.end_capacity = vector.end;
    return vector;
}

static inline union std_string new_std_string_short_mode(const char *str) {
    short str_size = strlen(str);
    union std_string std_str = {
        .mark = str_size << 1,
    };
    strcpy(std_str.str, str);
    return std_str;
}

static inline const char *std_string_data(union std_string *str) {
	if ((str->mark & 1) == 0) {
		return str->str;
	}
	return str->data;
}

extern void curl_easy_setopt(void *curl, int32_t option, ...);
extern void *curl_easy_init();
extern void *curl_easy_perform(void *);
extern void curl_easy_cleanup(void *);

extern void _ZN20androidstoreservices30SVSubscriptionStatusMgrFactory6createEv(struct shared_ptr *);
extern void
_ZN20androidstoreservices27SVSubscriptionStatusMgrImpl33checkSubscriptionStatusFromSourceERKNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEEERKNS_23SVSubscriptionStatusMgr26SVSubscriptionStatusSourceE(
	struct shared_ptr *, void *, struct shared_ptr *, int *);
extern void
_ZN17storeservicescore14RequestContext24setFairPlayDirectoryPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);

extern void _resolv_set_nameservers_for_net(unsigned netid, const char **servers, int numservers, const char *domains);

extern void
_ZN14FootHillConfig6configERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE(union std_string *);

extern void
_ZNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEE11make_sharedIJRNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEEEES3_DpOT_(
	struct shared_ptr *, union std_string *);
extern void _ZNSt6__ndk110shared_ptrIN20androidstoreservices28AndroidPresentationInterfaceEE11make_sharedIJEEES3_DpOT_(
	struct shared_ptr *);

extern void
_ZN20androidstoreservices28AndroidPresentationInterface16setDialogHandlerEPFvlNSt6__ndk110shared_ptrIN17storeservicescore14ProtocolDialogEEENS2_INS_36AndroidProtocolDialogResponseHandlerEEEE(
	void *, void (*)(long, struct shared_ptr *, struct shared_ptr *));
extern void
_ZN20androidstoreservices28AndroidPresentationInterface21setCredentialsHandlerEPFvNSt6__ndk110shared_ptrIN17storeservicescore18CredentialsRequestEEENS2_INS_33AndroidCredentialsResponseHandlerEEEE(
	void *, void (*)(struct shared_ptr *, struct shared_ptr *));

extern void
_ZN17storeservicescore14RequestContext24setPresentationInterfaceERKNSt6__ndk110shared_ptrINS_21PresentationInterfaceEEE(
	void *, struct shared_ptr *);

extern void
_ZNSt6__ndk110shared_ptrIN17storeservicescore16AuthenticateFlowEE11make_sharedIJRNS0_INS1_14RequestContextEEEEEES3_DpOT_(
	struct shared_ptr *, struct shared_ptr *);
extern void _ZN17storeservicescore16AuthenticateFlow3runEv(void *);
extern struct shared_ptr *_ZNK17storeservicescore16AuthenticateFlow8responseEv(void *);
extern int _ZNK17storeservicescore20AuthenticateResponse12responseTypeEv(void *);

// storeservicescore::PlaybackLeaseSession::PlaybackLeaseSession(std::__ndk1::shared_ptr<storeservicescore::RequestContext>
// const&)
extern void
_ZN22SVPlaybackLeaseManagerC2ERKNSt6__ndk18functionIFvRKiEEERKNS1_IFvRKNS0_10shared_ptrIN17storeservicescore19StoreErrorConditionEEEEEE(
	void *, void *, void *);
extern void _ZN22SVPlaybackLeaseManager25refreshLeaseAutomaticallyERKb(void *, uint8_t *);
extern void _ZN22SVPlaybackLeaseManager12requestLeaseERKb(void *, uint8_t *);

// storeservicescore::ProtocolDialogResponse::setSelectedButton(std::__ndk1::shared_ptr<storeservicescore::ProtocolButton>
// const&)
extern void
_ZN17storeservicescore22ProtocolDialogResponse17setSelectedButtonERKNSt6__ndk110shared_ptrINS_14ProtocolButtonEEE(
	void *, struct shared_ptr *);
// storeservicescore::ProtocolDialog::title() const
extern union std_string *_ZNK17storeservicescore14ProtocolDialog5titleEv(void *);
// storeservicescore::ProtocolDialog::message() const
extern union std_string *_ZNK17storeservicescore14ProtocolDialog7messageEv(void *);

extern union std_string *_ZNK17storeservicescore18CredentialsRequest5titleEv(void *);
extern union std_string *_ZNK17storeservicescore18CredentialsRequest7messageEv(void *);
extern uint8_t _ZNK17storeservicescore18CredentialsRequest28requiresHSA2VerificationCodeEv(void *);

extern void
_ZN20androidstoreservices28AndroidPresentationInterface28handleProtocolDialogResponseERKlRKNSt6__ndk110shared_ptrIN17storeservicescore22ProtocolDialogResponseEEE(
	void *, long *j, struct shared_ptr *);
// androidstoreservices::AndroidPresentationInterface::handleCredentialsResponse(std::__ndk1::shared_ptr<storeservicescore::CredentialsResponse>
// const&)
extern void
_ZN20androidstoreservices28AndroidPresentationInterface25handleCredentialsResponseERKNSt6__ndk110shared_ptrIN17storeservicescore19CredentialsResponseEEE(
	void *, struct shared_ptr *);

extern void _ZN17storeservicescore22ProtocolDialogResponseC1Ev(void *);

// storeservicescore::CredentialsResponse::CredentialsResponse()
extern void _ZN17storeservicescore19CredentialsResponseC1Ev(void *);
// storeservicescore::CredentialsResponse::setUserName(std::__ndk1::basic_string<char,
// std::__ndk1::char_traits<char>, std::__ndk1::allocator<char> > const&)
extern void
_ZN17storeservicescore19CredentialsResponse11setUserNameERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);
// storeservicescore::CredentialsResponse::setPassword(std::__ndk1::basic_string<char,
// std::__ndk1::char_traits<char>, std::__ndk1::allocator<char> > const&)
extern void
_ZN17storeservicescore19CredentialsResponse11setPasswordERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);
// storeservicescore::CredentialsResponse::setResponseType(storeservicescore::CredentialsResponse::ResponseType)
extern void _ZN17storeservicescore19CredentialsResponse15setResponseTypeENS0_12ResponseTypeE(void *, int responseType);

// storeservicescore::AuthenticateFlow::response() const
extern struct shared_ptr *_ZNK17storeservicescore16AuthenticateFlow8responseEv(void *);

extern struct std_vector *_ZNK17storeservicescore14ProtocolDialog7buttonsEv(void *);
extern union std_string *_ZNK17storeservicescore14ProtocolButton5titleEv(void *);

// storeservicescore::DeviceGUID::instance()
extern void _ZN17storeservicescore10DeviceGUID8instanceEv(struct shared_ptr *);
// storeservicescore::DeviceGUID::configure(std::__ndk1::basic_string<char,
// std::__ndk1::char_traits<char>, std::__ndk1::allocator<char> > const&,
// std::__ndk1::basic_string<char, std::__ndk1::char_traits<char>,
// std::__ndk1::allocator<char> > const&, unsigned int const&, bool const&)
extern void
_ZN17storeservicescore10DeviceGUID9configureERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_RKjRKb(
	void *, void *, union std_string *, union std_string *, unsigned int *, uint8_t *);

// mediaplatform::DebugLogEnabledForPriority(mediaplatform::LogPriority)
extern uint8_t _ZN13mediaplatform26DebugLogEnabledForPriorityENS_11LogPriorityE();

extern void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore22ProtocolDialogResponseENS_9allocatorIS2_EEEE;
extern void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore19CredentialsResponseENS_9allocatorIS2_EEEE;
// vtable for
// std::__ndk1::__shared_ptr_emplace<storeservicescore::RequestContextConfig,
// std::__ndk1::allocator<storeservicescore::RequestContextConfig> >
extern void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN17storeservicescore20RequestContextConfigENS_9allocatorIS2_EEEE;

// storeservicescore::RequestContextConfig::RequestContextConfig()
extern void _ZN17storeservicescore20RequestContextConfigC2Ev(void *);
extern void _ZN17storeservicescore20RequestContextConfig9setCPFlagEb(void *, uint8_t);
extern void
_ZN17storeservicescore20RequestContextConfig20setBaseDirectoryPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);
// storeservicescore::RequestContextConfig::setClientIdentifier(std::__ndk1::basic_string<char,
// std::__ndk1::char_traits<char>, std::__ndk1::allocator<char> > const&)
extern void
_ZN17storeservicescore20RequestContextConfig19setClientIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);
extern void
_ZN17storeservicescore20RequestContextConfig20setVersionIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);
extern void
_ZN17storeservicescore20RequestContextConfig21setPlatformIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);
extern void
_ZN17storeservicescore20RequestContextConfig17setProductVersionERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);
extern void
_ZN17storeservicescore20RequestContextConfig14setDeviceModelERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);
extern void
_ZN17storeservicescore20RequestContextConfig15setBuildVersionERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);
extern void
_ZN17storeservicescore20RequestContextConfig19setLocaleIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);
extern void
_ZN17storeservicescore20RequestContextConfig21setLanguageIdentifierERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);
extern void
_ZN17storeservicescore20RequestContextConfig24setFairPlayDirectoryPathERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE(
	void *, union std_string *);

extern void
_ZN17storeservicescore14RequestContext4initERKNSt6__ndk110shared_ptrINS_20RequestContextConfigEEE(void *, void *,
																								  struct shared_ptr *);

extern void *_ZN21SVFootHillSessionCtrl8instanceEv();
extern void *_ZN21SVFootHillSessionCtrl7destroyEv();
extern void _ZN21SVFootHillSessionCtrl9cleanKeysERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE(
	void *, union std_string *);
extern void
_ZN21SVFootHillSessionCtrl16getPersistentKeyERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_S8_S8_S8_S8_S8_S8_(
	struct shared_ptr *, void *, union std_string *, union std_string *, union std_string *, union std_string *,
	union std_string *, union std_string *, union std_string *, union std_string *);
extern void
_ZN21SVFootHillSessionCtrl14decryptContextERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEERKN11SVDecryptor15SVDecryptorTypeERKb(
	struct shared_ptr *ret, void *sessCtrlInstance, union std_string *ckc);
extern void _ZNSt6__ndk110shared_ptrI18SVFootHillPContextED2Ev(struct shared_ptr *);
extern void **_ZNK18SVFootHillPContext9kdContextEv(void *);
extern long NfcRKVnxuKZy04KWbdFu71Ou(void *, uint32_t, void *, void *, size_t);

extern void _ZN8FootHillC2ERKNSt6__ndk112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEES8_(void *,
																									union std_string *,
																									union std_string *);
extern void _ZN8FootHill24defaultContextIdentifierEv(void *);

// RequestContextManager::configure(std::__ndk1::shared_ptr<storeservicescore::RequestContext>
// const&)
extern void _ZN21RequestContextManager9configureERKNSt6__ndk110shared_ptrIN17storeservicescore14RequestContextEEE(
	struct shared_ptr *);

extern struct shared_ptr *_ZN22SVPlaybackLeaseManager12requestAssetERKmRKNSt6__ndk16vectorINS2_12basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEENS7_IS9_EEEERKb(
	void *, void *, unsigned long *, struct std_vector *, uint8_t *
);
extern int _ZNK23SVPlaybackAssetResponse13hasValidAssetEv(void *);
extern struct shared_ptr *_ZNK23SVPlaybackAssetResponse13playbackAssetEv(void *);
extern union std_string *_ZNK17storeservicescore13PlaybackAsset9URLStringEv(void *, uint8_t *);

extern union std_string *_ZNK17storeservicescore14RequestContext20storeFrontIdentifierERKNSt6__ndk110shared_ptrINS_6URLBagEEE(void *, void *, struct shared_ptr *);

extern void _ZN21SVFootHillSessionCtrl16resetAllContextsEv(void *);
extern int __android_log_print(int prio, const char *tag, const char *fmt, ...);
extern int __android_log_write(int prio, const char *tag, const char *text);

extern void *_ZTVNSt6__ndk120__shared_ptr_emplaceIN13mediaplatform11HTTPMessageENS_9allocatorIS2_EEEE;
extern void *_ZN13mediaplatform11HTTPMessageC2ENSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES7_(void *,  union std_string *, union std_string *);
extern void _ZN13mediaplatform11HTTPMessage9setHeaderERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_(void *, union std_string *, union std_string *);
extern void _ZN13mediaplatform11HTTPMessage11setBodyDataEPcm(void *,char *, u_long);
extern void *_ZN17storeservicescore10DeviceGUID4guidEv(void *, void *);
extern char *_ZNK13mediaplatform4Data5bytesEv(void *);
extern void *_ZN17storeservicescore10URLRequestC2ERKNSt6__ndk110shared_ptrIN13mediaplatform11HTTPMessageEEERKNS2_INS_14RequestContextEEE(void *, struct shared_ptr *, struct shared_ptr *);
extern void *_ZN17storeservicescore10URLRequest19setRequestParameterERKNSt6__ndk112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_(void *, union std_string *, union std_string *);
extern void *_ZN17storeservicescore10URLRequest3runEv(void *);
extern struct shared_ptr *_ZNK17storeservicescore10URLRequest5errorEv(void *);
extern struct shared_ptr *_ZNK17storeservicescore10URLRequest8responseEv(void *);
extern struct shared_ptr *_ZNK17storeservicescore11URLResponse18underlyingResponseEv(void *);

const char *const android_id = "dc28071e981c439e";
const char *const fairplayCert = "MIIEzjCCA7agAwIBAgIIAXAVjHFZDjgwDQYJKoZIhvcNAQEFBQAwfzELMAkGA1UEBhMCVVMxEz"
								 "ARBgNVBAoMCkFwcGxlIEluYy4xJjAkBgNVBAsMHUFwcGxlIENlcnRpZmljYXRpb24gQXV0aG9y"
								 "aXR5MTMwMQYDVQQDDCpBcHBsZSBLZXkgU2VydmljZXMgQ2VydGlmaWNhdGlvbiBBdXRob3JpdH"
								 "kwHhcNMTIwNzI1MTgwMjU4WhcNMTQwNzI2MTgwMjU4WjAwMQswCQYDVQQGEwJVUzESMBAGA1UE"
								 "CgwJQXBwbGUgSW5jMQ0wCwYDVQQDDARGUFMxMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQ"
								 "CqZ9IbMt0J0dTKQN4cUlfeQRY9bcnbnP95HFv9A16Yayh4xQzRLAQqVSmisZtBK2/nawZcDmcs"
								 "+XapBojRb+jDM4Dzk6/Ygdqo8LoA+BE1zipVyalGLj8Y86hTC9QHX8i05oWNCDIlmabjjWvFBo"
								 "EOk+ezOAPg8c0SET38x5u+TwIDAQABo4ICHzCCAhswHQYDVR0OBBYEFPP6sfTWpOQ5Sguf5W3Y"
								 "0oibbEc3MAwGA1UdEwEB/wQCMAAwHwYDVR0jBBgwFoAUY+RHVMuFcVlGLIOszEQxZGcDLL4wge"
								 "IGA1UdIASB2jCB1zCB1AYJKoZIhvdjZAUBMIHGMIHDBggrBgEFBQcCAjCBtgyBs1JlbGlhbmNl"
								 "IG9uIHRoaXMgY2VydGlmaWNhdGUgYnkgYW55IHBhcnR5IGFzc3VtZXMgYWNjZXB0YW5jZSBvZi"
								 "B0aGUgdGhlbiBhcHBsaWNhYmxlIHN0YW5kYXJkIHRlcm1zIGFuZCBjb25kaXRpb25zIG9mIHVz"
								 "ZSwgY2VydGlmaWNhdGUgcG9saWN5IGFuZCBjZXJ0aWZpY2F0aW9uIHByYWN0aWNlIHN0YXRlbW"
								 "VudHMuMDUGA1UdHwQuMCwwKqAooCaGJGh0dHA6Ly9jcmwuYXBwbGUuY29tL2tleXNlcnZpY2Vz"
								 "LmNybDAOBgNVHQ8BAf8EBAMCBSAwFAYLKoZIhvdjZAYNAQUBAf8EAgUAMBsGCyqGSIb3Y2QGDQ"
								 "EGAQH/BAkBAAAAAQAAAAEwKQYLKoZIhvdjZAYNAQMBAf8EFwF+bjsY57ASVFmeehD2bdu6HLGB"
								 "xeC2MEEGCyqGSIb3Y2QGDQEEAQH/BC8BHrKviHJf/Se/ibc7T0/55Bt1GePzaYBVfgF3ZiNuV9"
								 "3z8P3qsawAqAXzzh9o5DANBgkqhkiG9w0BAQUFAAOCAQEAVGyCtuLYcYb/aPijBCtaemxuV0Io"
								 "kXJn3EgmwYHZynaR6HZmeGRUp9p3f8EXu6XPSekKCCQi+a86hXX9RfnGEjRdvtP+jts5MDSKuU"
								 "Ioaqce8cLX2dpUOZXdf3lR0IQM0kXHb5boNGBsmbTLVifqeMsexfZryGw2hE/4WDOJdGQm1gMJ"
								 "ZU4jP1b/HSLNIUhHWAaMeWtcJTPRBucR4urAtvvtOWD88mriZNHG+veYw55b+qA36PSqDPMbku"
								 "9xTY7fsMa6mxIRmwULQgi8nOk1wNhw3ZO0qUKtaCO3gSqWdloecxpxUQSZCSW7tWPkpXXwDZqe"
								 "gUkij9xMFS1pr37RIjCCBVAwggQ4oAMCAQICEEVKuaGraq1Cp4z6TFOeVfUwDQYJKoZIhvcNAQ"
								 "ELBQAwUDEsMCoGA1UEAwwjQXBwbGUgRlAgU2VydmljZSBFbmFibGUgUlNBIENBIC0gRzExEzAR"
								 "BgNVBAoMCkFwcGxlIEluYy4xCzAJBgNVBAYTAlVTMB4XDTIwMDQwNzIwMjY0NFoXDTIyMDQwNz"
								 "IwMjY0NFowWjEhMB8GA1UEAwwYZnBzMjA0OC5pdHVuZXMuYXBwbGUuY29tMRMwEQYDVQQLDApB"
								 "cHBsZSBJbmMuMRMwEQYDVQQKDApBcHBsZSBJbmMuMQswCQYDVQQGEwJVUzCCASIwDQYJKoZIhv"
								 "cNAQEBBQADggEPADCCAQoCggEBAJNoUHuTRLafofQgIRgGa2TFIf+bsFDMjs+y3Ep1xCzFLE4Q"
								 "bnwG6OG0duKUl5IoGUsouzZk9iGsXz5k3ESLOWKz2BFrDTvGrzAcuLpH66jJHGsk/l+ZzsDOJa"
								 "oQ22pu0JvzYzW8/yEKvpE6JF/2dsC6V9RDTri3VWFxrl5uh8czzncoEQoRcQsSatHzs4tw/QdH"
								 "FtBIigqxqr4R7XiCaHbsQmqbP9h7oxRs/6W/DDA2BgkuFY1ocX/8dTjmH6szKPfGt3KaYCwy3f"
								 "uRC+FibTyohtvmlXsYhm7AUzorwWIwN/MbiFQ0OHHtDomIy71wDcTNMnY0jZYtGmIlJETAgYcC"
								 "AwEAAaOCAhowggIWMAwGA1UdEwEB/wQCMAAwHwYDVR0jBBgwFoAUrI/yBkpV623/IeMrXzs8fC"
								 "7VkZkwRQYIKwYBBQUHAQEEOTA3MDUGCCsGAQUFBzABhilodHRwOi8vb2NzcC5hcHBsZS5jb20v"
								 "b2NzcDAzLWZwc3J2cnNhZzEwMzCBwwYDVR0gBIG7MIG4MIG1BgkqhkiG92NkBQEwgacwgaQGCC"
								 "sGAQUFBwICMIGXDIGUUmVsaWFuY2Ugb24gdGhpcyBjZXJ0aWZpY2F0ZSBieSBhbnkgcGFydHkg"
								 "YXNzdW1lcyBhY2NlcHRhbmNlIG9mIGFueSBhcHBsaWNhYmxlIHRlcm1zIGFuZCBjb25kaXRpb2"
								 "5zIG9mIHVzZSBhbmQvb3IgY2VydGlmaWNhdGlvbiBwcmFjdGljZSBzdGF0ZW1lbnRzLjAdBgNV"
								 "HQ4EFgQU2RpCSSHFXeoZQQWxbwJuRZ9RrIEwDgYDVR0PAQH/BAQDAgUgMBQGCyqGSIb3Y2QGDQ"
								 "EFAQH/BAIFADAjBgsqhkiG92NkBg0BBgEB/wQRAQAAAAMAAAABAAAAAgAAAAMwOQYLKoZIhvdj"
								 "ZAYNAQMBAf8EJwG+pUeWbeZBUI0PikyFwSggL5dHaeugSDoQKwcP28csLuh5wplpATAzBgsqhk"
								 "iG92NkBg0BBAEB/wQhAfl9TGjP/UY9TyQzYsn8sX9ZvHChok9QrrUhtAyWR1yCMA0GCSqGSIb3"
								 "DQEBCwUAA4IBAQBNMzZ6llQ0laLXsrmyVieuoW9+pHeAaDJ7cBiQLjM3ZdIO3Gq5dkbWYYYwJw"
								 "ymdxZ74WGZMuVv3ueJKcxG1jAhCRhr0lb6QaPaQQSNW+xnoesb3CLA0RzrcgBp/9WFZNdttJOS"
								 "yC93lQmiE0r5RqPpe/IWUzwoZxri8qnsghVFxCBEcMB+U4PJR8WeAkPrji8po2JLYurvgNRhGk"
								 "DKcAFPuGEpXdF86hPts+07zazsP0fBjBSVgP3jqb8G31w5W+O+wBW0B9uCf3s0vXU4LuJTAyww"
								 "s2ImZ7O/AaY/uXWOyIUMUKPgL1/QJieB7pBoENIJ2CeJS2M3iv00ssmCmTEJ";