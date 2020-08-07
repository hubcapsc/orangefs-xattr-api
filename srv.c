#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pvfs2.h>
#include <pvfs2-util.h>

#define KEYBUFSIZE 256
/*
#define VALBUFSIZE 64 * 1024	*/ /* vfs limit, Orangefs limit = ? */
#define VALBUFSIZE PVFS_MAX_XATTR_VALUELEN	/* found it <g> */

#define USAGE "Usage: %s [get|del|set|enum] file [key] [value]\n"
/*
  ./srv get /pvfsmnt/testfile key
  ./srv del /pvfsmnt/testfile key
  ./srv set /pvfsmnt/testfile key value
  ./srv enum /pvfsmnt/testfile
*/

int orange_xattr(char *, char *, char *, char *);

/* not public? */
int PINT_remove_base_dir(const char *pathname, char *out_dir, int out_max_len);
int PINT_lookup_parent(char *filename,
                       PVFS_fs_id fs_id,
                       PVFS_credential *credential,
                       PVFS_handle * handle);

int main(int argc, char *argv[]) {
	int rc;

	if ((argc != 3 ) && (argc != 4) && (argc != 5))
		goto usage;

	if (strcmp(argv[1],"get") && 
	    strcmp(argv[1],"del") && 
	    strcmp(argv[1],"set") &&
	    strcmp(argv[1],"enum"))
		goto usage;

	if (!strcmp(argv[1], "get") || !strcmp(argv[1], "del"))
		if (argv[3])
			rc = orange_xattr(argv[1], argv[2], argv[3], NULL);
		else
			goto usage;

	if (!strcmp(argv[1], "set")) {
		if (argv[4])
			rc = orange_xattr(argv[1], argv[2], argv[3], argv[4]);
		else
			goto usage;
	}

	if (!strcmp(argv[1], "enum"))
		rc = orange_xattr(argv[1], argv[2], NULL, NULL);

	exit(1);

usage:
	printf(USAGE,argv[0]);
	exit(0);
}

int orange_xattr(char *action, char *object, char *key_name, char *key_value) {
	int rc;
	char orangefs_path[PVFS_NAME_MAX];
	PVFS_credential orangefs_credential;
	PVFS_fs_id cur_fs;
	PVFS_ds_keyval key;
	PVFS_ds_keyval val;
	char valbuf[VALBUFSIZE];
	PVFS_sysresp_lookup resp_lookup;
	int nkey = PVFS_MAX_XATTR_LISTLEN;
	PVFS_ds_keyval *keyp;
	PVFS_sysresp_listeattr resp_listeattr;
	PVFS_object_ref parent_ref;
	char str_buf[PVFS_NAME_MAX] = {0};
	int i;
	PVFS_ds_position token = PVFS_ITERATE_START;

	pvfs2_acl_header *ph;

	memset(&key, 0, sizeof(key));
	memset(&val, 0, sizeof(val));

	/* Generic initialization. */
	rc = PVFS_util_init_defaults();
	if (rc < 0) {
		printf("PVFS_util_init_defaults failed, rc:%d:\n", rc);
		return(rc);
	}

	memset(orangefs_path, 0, PVFS_NAME_MAX);

	/* Strip the mountpoint off the absolute path. */
	rc = PVFS_util_resolve(object, &cur_fs, orangefs_path, PVFS_NAME_MAX);
	if (rc != 0) {
		printf("%s: PVFS_util_resolve failed on %s, rc:%d:\n",
			__func__,
			object,
			rc);
		return(rc);
	}
	if (!strlen(orangefs_path)) {
		printf("%s: can't operate on the mount point.\n", __func__);
		return(-1);
	}

	/* Generate a signed credential for the current user. */
	rc = PVFS_util_gen_credential_defaults(&orangefs_credential); 
	if (rc != 0) {
		printf("%s: PVFS_util_gen_credential_defaults rc:%d:\n",
			__func__,
			rc);
		return(rc);
	}

	/* Lookup the object. */
	memset(&resp_lookup, 0, sizeof(PVFS_sysresp_lookup));
	rc = PVFS_sys_lookup(cur_fs,
			     orangefs_path,
			     &orangefs_credential,
			     &resp_lookup,
			     PVFS2_LOOKUP_LINK_FOLLOW,
			     NULL);
	if (rc) {
		printf("%s: PVFS_sys_lookup failed on %s, rc:%d:.\n",
			__func__,
			object,
			rc);
		return(rc);
	}

	/* key_name is supplied for get, del and set. */
	if (strcmp(action, "enum")) {
		key.buffer = key_name;
		key.buffer_sz = strlen(key_name) + 1;
	}

	/* set an extended attribute. */
	if (!strcmp(action,"set")) {
		val.buffer = key_value;
		val.buffer_sz = strlen(key_value) + 1;
		rc = PVFS_sys_seteattr(resp_lookup.ref,
					&orangefs_credential,
					&key,
					&val,
					0,
					NULL);
		if (rc) {
			printf("%s: PVFS_sys_seteattr :%s: :%d:\n",
				__func__,
				object,	
				rc);
		}

		goto out;
	}

	/* delete an extended attribute. */
	if (!strcmp(action,"del")) {
		rc = PVFS_sys_deleattr(resp_lookup.ref,
					&orangefs_credential,
					&key,
					NULL);
		if (rc) {
			printf("%s: PVFS_sys_deleattr :%s: :%d:\n",
				__func__,
				object,	
				rc);
		}

		goto out;
	}

	/* get an extended attribute. */
	if (!strcmp(action,"get")) {
		memset(&valbuf, 0, VALBUFSIZE);
		val.buffer = &valbuf;
		val.buffer_sz = VALBUFSIZE;
		rc = PVFS_sys_geteattr(resp_lookup.ref,
					&orangefs_credential,
					&key,
					&val,
					NULL);
		if (rc) {
			printf("%s: PVFS_sys_geteattr :%s: :%d:\n",
				__func__,
				object,	
				rc);
			goto out;
		}

               /*
                * The value of an acl key has a header and some entries...
                *
                * typedef struct
                * {
                *    int16_t  p_tag;
                *    uint16_t p_perm;
                *    uint32_t p_id;
                * } pvfs2_acl_entry;
                * typedef struct 
                * {
                *     uint32_t p_version;
                *     pvfs2_acl_entry p_entries[0];
                * } pvfs2_acl_header;
                *
                * include/uapi/linux/posix_acl.h shows what p_tags are
                * and other stuff...
                */
		if (!strcmp(key.buffer, PVFS2_ACL_ACCESS) ||
		    !strcmp(key.buffer, PVFS2_ACL_DEFAULT)) {

			ph = (pvfs2_acl_header *) val.buffer;
			for (i = 0; ph->p_entries[i].p_tag; i++)
			printf("key:%s: ph->p_version:%d: "
			       "ph->p_entries[%d].p_tag:%d: "
			       "ph->p_entries[%d].p_perm:%x: "
			       "ph->p_entries[%d].p_id:%d: "
			       "val.buffer_sz:%d:\n",
				key.buffer,
				ph->p_version,
				i, ph->p_entries[i].p_tag,
				i, ph->p_entries[i].p_perm,
				i, ph->p_entries[i].p_id,
				val.buffer_sz);
		} else {
		/* print off key/val pairs of unspecified type... */
			printf("key:%s: value:%s:\n",
				key.buffer,
				val.buffer);
		}

		goto out;
	}

	/* must be an enumeration. */
	if (strcmp(action,"enum")) {
		printf("%s: action:%s:\n", __func__, action);
		goto out;
	}

	keyp = (PVFS_ds_keyval *)malloc(sizeof(PVFS_ds_keyval) * nkey);
	if (!keyp) {
		printf("%s: malloc failed.\n", __func__);
		goto out;
	}

	for (i = 0; i < nkey; i++) {
		keyp[i].buffer_sz = KEYBUFSIZE;
		keyp[i].buffer = (char*)malloc(sizeof(char)*keyp[i].buffer_sz);
		if (!keyp[i].buffer) {
			printf("%s: malloc failed, i:%d:\n", __func__, i);
			goto out;
		}
	}

	rc = PINT_remove_base_dir(orangefs_path, str_buf,PVFS_NAME_MAX);
	if (rc) {
		printf("%s: PINT_remove_base_dir, rc:%d:\n", __func__, rc);
		exit(-1);
	}

	rc = PINT_lookup_parent(orangefs_path,
				cur_fs,
				&orangefs_credential,
				&parent_ref.handle);
	if (rc) {
		printf("%s: PINT_lookup_parent, rc:%d:\n", __func__, rc);
		goto out;
	}

	parent_ref.fs_id=cur_fs;

	memset(&resp_lookup, 0, sizeof(PVFS_sysresp_lookup));
	rc = PVFS_sys_ref_lookup(parent_ref.fs_id,
				str_buf,
				parent_ref,
				&orangefs_credential,
				&resp_lookup,
				PVFS2_LOOKUP_LINK_NO_FOLLOW,
				NULL);

	if (rc != 0) {
		printf("%s: PVFS_sys_ref_lookup failed on %s.\n",
			__func__,
			str_buf);
		goto out;
	}

	resp_listeattr.key_array = keyp;
	resp_listeattr.token = 0;

	while (resp_listeattr.token != PVFS_ITERATE_END) {

		rc = PVFS_sys_listeattr(resp_lookup.ref,
					token,
					nkey,
					&orangefs_credential,
					&resp_listeattr,
					NULL);

		for (i = 0; i < resp_listeattr.nkey; i++)
			printf(":%s:\n", keyp[i].buffer);

		token = resp_listeattr.token;
	}

out:
	PVFS_sys_finalize();
	return(rc);
}
