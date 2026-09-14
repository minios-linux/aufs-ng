/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Kernel-version shims.  Every LINUX_VERSION_CODE guard lives here so
 * version churn stays out of the filesystem code.
 */
#ifndef AUFSNG_COMPAT_H
#define AUFSNG_COMPAT_H

#include <linux/version.h>
#include <linux/namei.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
#error "aufs-ng supports kernel 6.12 and later"
#endif

#ifndef QSTR_LEN
#define QSTR_LEN(n, l) ((struct qstr)QSTR_INIT(n, l))
#endif

/* Linux 6.12 passes this internal flag down to ->getattr(). */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 13, 0)
static inline int aufsng_vfs_getattr(const struct path *path,
				     struct kstat *stat, u32 request_mask,
				     unsigned int flags)
{
	if (flags & AT_GETATTR_NOSEC)
		return vfs_getattr_nosec(path, stat, request_mask, flags);
	return vfs_getattr(path, stat, request_mask, flags);
}
#else
#define aufsng_vfs_getattr vfs_getattr
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0)
#define AUFSNG_D_REVALIDATE_COMPAT(fn) \
static int fn##_compat(struct dentry *dentry, unsigned int flags) \
{ \
	return fn(d_inode(dentry->d_parent), &dentry->d_name, dentry, flags); \
}
#define AUFSNG_D_REVALIDATE_FN(fn) fn##_compat
#else
#define AUFSNG_D_REVALIDATE_COMPAT(fn)
#define AUFSNG_D_REVALIDATE_FN(fn) fn
#endif

static inline int aufsng_lower_d_revalidate(struct dentry *dentry,
					    unsigned int flags)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0)
	return dentry->d_op->d_revalidate(dentry, flags);
#else
	return dentry->d_op->d_revalidate(d_inode(dentry->d_parent),
					  &dentry->d_name, dentry, flags);
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
#define inode_state_read_once(inode) READ_ONCE((inode)->i_state)
#define inode_state_read(inode) ((inode)->i_state)
#define inode_state_set(inode, flags) \
	WRITE_ONCE((inode)->i_state, (inode)->i_state | (flags))
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 15, 0)
#define AUFSNG_MKDIR_COMPAT(fn) \
static int fn##_compat(struct mnt_idmap *idmap, struct inode *dir, \
		       struct dentry *dentry, umode_t mode) \
{ \
	return PTR_ERR_OR_ZERO(fn(idmap, dir, dentry, mode)); \
}
#define AUFSNG_MKDIR_FN(fn) fn##_compat
#else
#define AUFSNG_MKDIR_COMPAT(fn)
#define AUFSNG_MKDIR_FN(fn) fn
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 16, 0)
#define AUFSNG_DIR_CONTEXT_COUNT(member)

static inline struct dentry *
aufsng_lookup_one(struct mnt_idmap *idmap, const struct qstr *name,
		  struct dentry *base)
{
	return lookup_one(idmap, name->name, base, name->len);
}

#define lookup_one(idmap, name, base) aufsng_lookup_one(idmap, name, base)
#else
#define AUFSNG_DIR_CONTEXT_COUNT(member) .member.count = INT_MAX,
#endif

/*
 * Linux 6.12.95 stable backported the backing_file_open() LSM change
 * from 7.1, while mainline 6.13..7.0 retain the path-based argument.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 1, 0) || \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 95) && \
	 LINUX_VERSION_CODE < KERNEL_VERSION(6, 13, 0))
static inline struct file *
aufsng_backing_file_open(struct file *file, int flags,
			 const struct path *realpath, const struct cred *cred)
{
	return backing_file_open(file, flags, realpath, cred);
}
#else
static inline struct file *
aufsng_backing_file_open(struct file *file, int flags,
			 const struct path *realpath, const struct cred *cred)
{
	return backing_file_open(&file->f_path, flags, realpath, cred);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
#define AUFSNG_END_WRITE_ARGS \
	struct file *file, loff_t pos, ssize_t ret
#define AUFSNG_END_WRITE_FILE file

static inline void aufsng_backing_ctx_set_user_file(struct backing_file_ctx *ctx,
					     struct file *file)
{
	ctx->user_file = file;
}

static inline ssize_t
aufsng_backing_file_splice_read(struct file *in, struct kiocb *iocb,
				struct pipe_inode_info *pipe, size_t len,
				unsigned int flags, struct backing_file_ctx *ctx)
{
	ssize_t ret = backing_file_splice_read(in, &iocb->ki_pos, pipe, len,
					       flags, ctx);

	return ret;
}

static inline ssize_t
aufsng_backing_file_splice_write(struct pipe_inode_info *pipe, struct file *out,
				 struct kiocb *iocb, size_t len,
				 unsigned int flags, struct backing_file_ctx *ctx)
{
	return backing_file_splice_write(pipe, out, &iocb->ki_pos, len, flags, ctx);
}
#else
#define AUFSNG_END_WRITE_ARGS struct kiocb *iocb, ssize_t ret
#define AUFSNG_END_WRITE_FILE (iocb->ki_filp)

static inline void aufsng_backing_ctx_set_user_file(struct backing_file_ctx *ctx,
					     struct file *file)
{
}

#define aufsng_backing_file_splice_read backing_file_splice_read
#define aufsng_backing_file_splice_write backing_file_splice_write
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 17, 0)
#define set_default_d_op(sb, op) ((sb)->s_d_op = (op))
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(7, 0, 0)
#define name_is_dot_dotdot(name, len) is_dot_dotdot(name, len)

static inline int aufsng_lookup_noperm_common(struct qstr *qname,
					      struct dentry *base)
{
	const char *name = qname->name;
	u32 len = qname->len;

	qname->hash = full_name_hash(base, name, len);
	if (!len || is_dot_dotdot(name, len))
		return -EACCES;
	while (len--) {
		unsigned int c = *(const unsigned char *)name++;

		if (c == '/' || c == '\0')
			return -EACCES;
	}
	if (base->d_flags & DCACHE_OP_HASH) {
		int err = base->d_op->d_hash(base, qname);

		if (err < 0)
			return err;
	}
	return 0;
}

static inline int aufsng_lookup_common(struct mnt_idmap *idmap,
				       struct qstr *qname,
				       struct dentry *base)
{
	int err = aufsng_lookup_noperm_common(qname, base);

	return err ? err : inode_permission(idmap, d_inode(base), MAY_EXEC);
}

static inline struct dentry *
aufsng_start_dirop(struct dentry *parent, struct qstr *name,
		   unsigned int lookup_flags)
{
	struct inode *dir = d_inode(parent);
	struct dentry *dentry;
	int err;

	err = aufsng_lookup_noperm_common(name, parent);
	if (err)
		return ERR_PTR(err);
	inode_lock_nested(dir, I_MUTEX_PARENT);
	dentry = lookup_one_qstr_excl(name, parent, lookup_flags);
	if (IS_ERR(dentry))
		goto out_unlock;
	if (!(lookup_flags & LOOKUP_CREATE) && d_is_negative(dentry)) {
		dput(dentry);
		dentry = ERR_PTR(-ENOENT);
		goto out_unlock;
	}
	if ((lookup_flags & LOOKUP_EXCL) && d_is_positive(dentry)) {
		dput(dentry);
		dentry = ERR_PTR(-EEXIST);
		goto out_unlock;
	}
	return dentry;

out_unlock:
	inode_unlock(dir);
	return dentry;
}

static inline struct dentry *
aufsng_start_creating_noperm(struct dentry *parent, struct qstr *name)
{
	return aufsng_start_dirop(parent, name, LOOKUP_CREATE);
}

static inline struct dentry *
aufsng_start_removing_noperm(struct dentry *parent, struct qstr *name)
{
	return aufsng_start_dirop(parent, name, 0);
}

static inline void aufsng_end_dirop(struct dentry *dentry)
{
	if (!IS_ERR(dentry)) {
		inode_unlock(d_inode(dentry->d_parent));
		dput(dentry);
	}
}

#define start_creating_noperm aufsng_start_creating_noperm
#define start_removing_noperm aufsng_start_removing_noperm
#define end_dirop aufsng_end_dirop

static inline int aufsng_vfs_create(struct mnt_idmap *idmap,
				    struct dentry *dentry, umode_t mode)
{
	return vfs_create(idmap, d_inode(dentry->d_parent), dentry, mode, true);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 15, 0)
static inline struct dentry *
aufsng_vfs_mkdir(struct mnt_idmap *idmap, struct inode *dir,
		 struct dentry *dentry, umode_t mode)
{
	int err = vfs_mkdir(idmap, dir, dentry, mode);

	if (err) {
		aufsng_end_dirop(dentry);
		return ERR_PTR(err);
	}
	return dentry;
}
#else
static inline struct dentry *
aufsng_vfs_mkdir(struct mnt_idmap *idmap, struct inode *dir,
		 struct dentry *dentry, umode_t mode)
{
	struct dentry *ret = vfs_mkdir(idmap, dir, dentry, mode);

	if (IS_ERR(ret))
		inode_unlock(dir);
	return ret;
}
#endif

static inline int aufsng_vfs_mknod(struct mnt_idmap *idmap, struct inode *dir,
				   struct dentry *dentry, umode_t mode,
				   dev_t dev)
{
	return vfs_mknod(idmap, dir, dentry, mode, dev);
}

static inline int aufsng_vfs_symlink(struct mnt_idmap *idmap,
				     struct inode *dir,
				     struct dentry *dentry,
				     const char *oldname)
{
	return vfs_symlink(idmap, dir, dentry, oldname);
}

static inline int aufsng_vfs_rmdir(struct mnt_idmap *idmap, struct inode *dir,
				   struct dentry *dentry)
{
	return vfs_rmdir(idmap, dir, dentry);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 17, 0)
static inline void aufsng_renamedata_init(struct renamedata *rd,
					  struct mnt_idmap *idmap,
					  struct dentry *old_parent,
					  struct dentry *new_parent)
{
	rd->old_mnt_idmap = idmap;
	rd->old_dir = d_inode(old_parent);
	rd->new_mnt_idmap = idmap;
	rd->new_dir = d_inode(new_parent);
}
#else
static inline void aufsng_renamedata_init(struct renamedata *rd,
					  struct mnt_idmap *idmap,
					  struct dentry *old_parent,
					  struct dentry *new_parent)
{
	rd->mnt_idmap = idmap;
	rd->old_parent = old_parent;
	rd->new_parent = new_parent;
}
#endif

static inline int
aufsng_start_renaming(struct renamedata *rd, struct mnt_idmap *idmap,
		      struct dentry *old_parent, struct dentry *new_parent,
		      int lookup_flags, struct qstr *old_last,
		      struct qstr *new_last)
{
	struct dentry *trap, *d1, *d2;
	int target_flags = LOOKUP_RENAME_TARGET | LOOKUP_CREATE;
	int err;

	aufsng_renamedata_init(rd, idmap, old_parent, new_parent);
	err = aufsng_lookup_common(idmap, old_last, old_parent);
	if (err)
		return err;
	err = aufsng_lookup_common(idmap, new_last, new_parent);
	if (err)
		return err;
	if (rd->flags & RENAME_EXCHANGE)
		target_flags = 0;
	if (rd->flags & RENAME_NOREPLACE)
		target_flags |= LOOKUP_EXCL;

	trap = lock_rename(old_parent, new_parent);
	if (IS_ERR(trap))
		return PTR_ERR(trap);
	d1 = lookup_one_qstr_excl(old_last, old_parent, lookup_flags);
	if (IS_ERR(d1)) {
		err = PTR_ERR(d1);
		goto out_unlock;
	}
	if (d_is_negative(d1)) {
		err = -ENOENT;
		goto out_dput_d1;
	}
	d2 = lookup_one_qstr_excl(new_last, new_parent,
				  lookup_flags | target_flags);
	if (IS_ERR(d2)) {
		err = PTR_ERR(d2);
		goto out_dput_d1;
	}
	if (d_is_negative(d2) && !(target_flags & LOOKUP_CREATE)) {
		err = -ENOENT;
		goto out_dput_d2;
	}
	if (d_is_positive(d2) && (target_flags & LOOKUP_EXCL)) {
		err = -EEXIST;
		goto out_dput_d2;
	}
	if (d1 == trap) {
		err = -EINVAL;
		goto out_dput_d2;
	}
	if (d2 == trap) {
		err = (rd->flags & RENAME_EXCHANGE) ? -EINVAL : -ENOTEMPTY;
		goto out_dput_d2;
	}
	rd->old_dentry = d1;
	rd->new_dentry = d2;
	return 0;

out_dput_d2:
	dput(d2);
out_dput_d1:
	dput(d1);
out_unlock:
	unlock_rename(old_parent, new_parent);
	return err;
}

static inline int
aufsng_start_renaming_dentry(struct renamedata *rd, struct mnt_idmap *idmap,
			     struct dentry *old_parent,
			     struct dentry *new_parent, int lookup_flags,
			     struct dentry *old_dentry,
			     struct qstr *new_last)
{
	struct dentry *trap, *d2;
	int target_flags = LOOKUP_RENAME_TARGET | LOOKUP_CREATE;
	int err;

	aufsng_renamedata_init(rd, idmap, old_parent, new_parent);
	err = aufsng_lookup_common(idmap, new_last, new_parent);
	if (err)
		return err;
	if (rd->flags & RENAME_EXCHANGE)
		target_flags = 0;
	if (rd->flags & RENAME_NOREPLACE)
		target_flags |= LOOKUP_EXCL;

	trap = lock_rename_child(old_dentry, new_parent);
	if (IS_ERR(trap))
		return PTR_ERR(trap);
	if (d_unhashed(old_dentry) || old_parent != old_dentry->d_parent) {
		err = -EINVAL;
		goto out_unlock;
	}
	d2 = lookup_one_qstr_excl(new_last, new_parent,
				  lookup_flags | target_flags);
	if (IS_ERR(d2)) {
		err = PTR_ERR(d2);
		goto out_unlock;
	}
	if (d_is_negative(d2) && !(target_flags & LOOKUP_CREATE)) {
		err = -ENOENT;
		goto out_dput_d2;
	}
	if (d_is_positive(d2) && (target_flags & LOOKUP_EXCL)) {
		err = -EEXIST;
		goto out_dput_d2;
	}
	if (old_dentry == trap) {
		err = -EINVAL;
		goto out_dput_d2;
	}
	if (d2 == trap) {
		err = (rd->flags & RENAME_EXCHANGE) ? -EINVAL : -ENOTEMPTY;
		goto out_dput_d2;
	}
	rd->old_dentry = dget(old_dentry);
	rd->new_dentry = d2;
	return 0;

out_dput_d2:
	dput(d2);
out_unlock:
	unlock_rename(old_dentry->d_parent, new_parent);
	return err;
}

static inline void aufsng_end_renaming(struct renamedata *rd,
				       struct dentry *old_parent,
				       struct dentry *new_parent)
{
	unlock_rename(old_parent, new_parent);
	dput(rd->old_dentry);
	dput(rd->new_dentry);
}
#else
static inline int aufsng_vfs_create(struct mnt_idmap *idmap,
				    struct dentry *dentry, umode_t mode)
{
	return vfs_create(idmap, dentry, mode, NULL);
}

static inline struct dentry *
aufsng_vfs_mkdir(struct mnt_idmap *idmap, struct inode *dir,
		 struct dentry *dentry, umode_t mode)
{
	return vfs_mkdir(idmap, dir, dentry, mode, NULL);
}

static inline int aufsng_vfs_mknod(struct mnt_idmap *idmap, struct inode *dir,
				   struct dentry *dentry, umode_t mode,
				   dev_t dev)
{
	return vfs_mknod(idmap, dir, dentry, mode, dev, NULL);
}

static inline int aufsng_vfs_symlink(struct mnt_idmap *idmap,
				     struct inode *dir,
				     struct dentry *dentry,
				     const char *oldname)
{
	return vfs_symlink(idmap, dir, dentry, oldname, NULL);
}

static inline int aufsng_vfs_rmdir(struct mnt_idmap *idmap, struct inode *dir,
				   struct dentry *dentry)
{
	return vfs_rmdir(idmap, dir, dentry, NULL);
}

static inline int
aufsng_start_renaming(struct renamedata *rd, struct mnt_idmap *idmap,
		      struct dentry *old_parent, struct dentry *new_parent,
		      int lookup_flags, struct qstr *old_last,
		      struct qstr *new_last)
{
	rd->mnt_idmap = idmap;
	rd->old_parent = old_parent;
	rd->new_parent = new_parent;
	return start_renaming(rd, lookup_flags, old_last, new_last);
}

static inline int
aufsng_start_renaming_dentry(struct renamedata *rd, struct mnt_idmap *idmap,
			     struct dentry *old_parent,
			     struct dentry *new_parent, int lookup_flags,
			     struct dentry *old_dentry,
			     struct qstr *new_last)
{
	rd->mnt_idmap = idmap;
	rd->old_parent = old_parent;
	rd->new_parent = new_parent;
	return start_renaming_dentry(rd, lookup_flags, old_dentry, new_last);
}

static inline void aufsng_end_renaming(struct renamedata *rd,
				       struct dentry *old_parent,
				       struct dentry *new_parent)
{
	end_renaming(rd);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 16, 0)
static inline struct dentry *
aufsng_lookup_one_positive_unlocked(struct mnt_idmap *idmap,
				    const struct qstr *name,
				    struct dentry *base)
{
	return lookup_one_positive_unlocked(idmap, name->name, base, name->len);
}

#define lookup_one_positive_unlocked(idmap, name, base) \
	aufsng_lookup_one_positive_unlocked(idmap, name, base)
#endif

#endif /* AUFSNG_COMPAT_H */
