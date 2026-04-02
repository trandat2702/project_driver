#ifndef AUTH_H
#define AUTH_H

void hash_sha256(const char *input, char *output_hex);
int  authenticate_credentials(const char *config_file,
							  const char *username,
							  const char *password);
int  authenticate(const char *config_file);

#endif
