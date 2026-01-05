#ifndef POLICY_H
#define POLICY_H

#define MAX_PATHS 32
#define MAX_LEN   256

/* Network policy modes */
typedef enum {
    NET_POLICY_DENY,    /* Deny all, allow only whitelisted */
    NET_POLICY_ALLOW    /* Allow all (testing mode) */
} NetworkPolicyMode;

typedef struct {
    /* File protection */
    char protected_files[MAX_PATHS][MAX_LEN];
    int protected_count;
    
    /* Network whitelist - domains or IPs */
    char network_whitelist[MAX_PATHS][MAX_LEN];
    int whitelist_count;
    
    /* Default network policy */
    NetworkPolicyMode network_mode;
    
    /* Allow all HTTPS (when domain filtering not possible) */
    int allow_all_https;
} Policy;

int load_policy(const char *filename, Policy *policy);
void print_policy(const Policy *policy);

#endif
