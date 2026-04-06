#ifndef AUTH_H
#define AUTH_H

#define MAX_USERS      64
#define MAX_USERNAME   64
#define MAX_ROLE_LEN   16

#define ROLE_ADMIN     "admin"
#define ROLE_VIEWER    "viewer"

void hash_sha256(const char *input, char *output_hex);

/* Authenticate and optionally retrieve the user's role.
   role_out may be NULL if caller doesn't need the role. */
int  authenticate_credentials(const char *config_file,
                              const char *username,
                              const char *password);

int  authenticate_with_role(const char *config_file,
                            const char *username,
                            const char *password,
                            char *role_out, size_t role_size);

int  authenticate(const char *config_file);

int  change_password(const char *config_file,
                     const char *username,
                     const char *old_password,
                     const char *new_password);

/* Get the role of a user (returns 0 on success, -1 if user not found) */
int  get_user_role(const char *config_file,
                   const char *username,
                   char *role_out, size_t role_size);

/* User management (admin only) */
int  add_user(const char *config_file,
              const char *username,
              const char *password,
              const char *role);

int  delete_user(const char *config_file,
                 const char *username);

/* List all users. Returns the count of users found.
   usernames[][MAX_USERNAME] and roles[][MAX_ROLE_LEN] must be pre-allocated. */
int  list_users(const char *config_file,
                char usernames[][MAX_USERNAME],
                char roles[][MAX_ROLE_LEN],
                int max_entries);

#endif
