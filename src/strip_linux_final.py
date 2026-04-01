import sys
import re

def main():
    with open('sed-opal.c', 'r') as f:
        content = f.read()

    # Prepend headers
    header_block = """
#include "linux_xcompat.h"
#include "opal_api.h"
#include "opal.h"
#include "opal_proto.h"
"""
    content = header_block + content

    # Cut off sed_ioctl
    idx = content.find('int sed_ioctl(struct opal_dev *dev')
    if idx != -1:
        content = content[:idx]

    # Remove Linux init functions
    content = re.sub(r'static int __init sed_opal_init.*', '', content, flags=re.DOTALL)

    # 1. Strip the keyring block manually without regex
    st1 = content.find('static int update_sed_opal_key')
    en1 = content.find('static bool check_tper')
    if st1 != -1 and en1 != -1:
        content = content[:st1] + 'static int opal_get_key(struct opal_dev *dev, struct opal_key *key) { return 0; }\n' + content[en1:]

    # Remove usages of keyring
    content = re.sub(r'\tret = sed_write_key[^;]+;[^\n]*\n', '', content)
    content = re.sub(r'ret = sed_write_key[^;]+;[^\n]*\n', '', content)
    content = re.sub(r'\t\tpr_warn\("error updating SED key: %d\\n", ret\);\n', '', content)
    content = re.sub(r'\n\tret = update_sed_opal_key\([^;]+;\n', '\n', content)
    
    # 2. Strip clean_opal_dev manually
    st2 = content.find('static void clean_opal_dev(struct opal_dev *dev)')
    en2 = content.find('void free_opal_dev(struct opal_dev *dev)')
    if st2 != -1 and en2 != -1:
        content = content[:st2] + 'static void clean_opal_dev(struct opal_dev *dev) { }\n\n' + content[en2:]
        
    # 3. Strip opal_lock_check_for_saved_key manually
    st3 = content.find('static void opal_lock_check_for_saved_key(struct opal_dev *dev,')
    en3 = content.find('static int opal_lock_unlock(struct opal_dev *dev,')
    if st3 != -1 and en3 != -1:
        content = content[:st3] + 'static void opal_lock_check_for_saved_key(struct opal_dev *dev, struct opal_lock_unlock *lk_unlk) { }\n\n' + content[en3:]

    # 4. Remove opal_unlock_from_suspend and opal_suspend manually
    st4 = content.find('bool opal_unlock_from_suspend(struct opal_dev *dev)')
    en4 = content.find('static int wait_for_tper_syncronization(struct opal_dev *dev)')
    if st4 != -1 and en4 != -1:
        content = content[:st4] + content[en4:]

    # Export functions
    funcs_to_export = [
        "opal_save",
        "opal_lock_unlock",
        "opal_take_ownership",
        "opal_activate_lsp",
        "opal_setup_locking_range",
        "opal_locking_range_status",
        "opal_set_new_pw",
        "opal_set_new_sid_pw",
        "opal_activate_user",
        "opal_reverttper",
        "opal_add_user_to_lr",
        "opal_enable_disable_shadow_mbr",
        "opal_set_mbr_done",
        "opal_write_shadow_mbr",
        "opal_erase_locking_range",
        "opal_secure_erase_locking_range",
        "opal_get_status",
        "opal_get_geometry",
        "opal_discovery0",
        "alloc_opal_dev",
        "free_opal_dev",
        "opal_generic_read_write_table"
    ]
    for method in funcs_to_export:
        content = re.sub(r'static int ' + method + r'\(', r'int ' + method + '(', content)
        content = re.sub(r'static void ' + method + r'\(', r'void ' + method + '(', content)
        content = re.sub(r'static struct opal_dev \*' + method + r'\(', r'struct opal_dev *' + method + '(', content)

    # Missing return in opal_set_new_pw because keyring update was the last statement
    content = re.sub(r'(int opal_set_new_pw\(.*?)\n}', r'\1\n\treturn ret;\n}', content, flags=re.DOTALL)

    # Remove the bad include <linux/...>
    content = re.sub(r'#include <linux/.*?>\n', '// removed\n', content)
    content = re.sub(r'#include <uapi/linux/.*?>\n', '// removed\n', content)
    content = re.sub(r'#include <keys/.*?>\n', '// removed\n', content)

    with open('opal.c', 'w') as f:
        f.write(content)

    with open('opal.h', 'w') as f:
        f.write("""#ifndef LINUX_OPAL_H
#define LINUX_OPAL_H
#include "opal_api.h"
#include <stdbool.h>
#include <stdint.h>
struct opal_dev;
typedef int (sec_send_recv)(void *data, uint16_t spsp, uint8_t secp, void *buffer, size_t len, bool send);
void free_opal_dev(struct opal_dev *dev);
struct opal_dev *init_opal_dev(void *data, sec_send_recv *send_recv);
#endif /* LINUX_OPAL_H */
""")

    with open('uapi_sed-opal.h', 'r') as f:
        api = f.read()
    api = re.sub(r'#include <linux/types.h>', '', api)
    api = re.sub(r'__u8', 'uint8_t', api)
    api = re.sub(r'__u16', 'uint16_t', api)
    api = re.sub(r'__u32', 'uint32_t', api)
    api = re.sub(r'__u64', 'uint64_t', api)
    api = re.sub(r'__be8', 'uint8_t', api)
    api = re.sub(r'__be16', 'uint16_t', api)
    api = re.sub(r'__be32', 'uint32_t', api)
    api = re.sub(r'__be64', 'uint64_t', api)
    api = re.sub(r'__le8', 'uint8_t', api)
    api = re.sub(r'__le16', 'uint16_t', api)
    api = re.sub(r'__le32', 'uint32_t', api)
    api = re.sub(r'__le64', 'uint64_t', api)
    api = re.sub(r'#define IOC_OPAL_.*?_IOW?.*?$|#define IOC_OPAL_.*?_IOR?.*?$', '', api, flags=re.MULTILINE)
    with open('opal_api.h', 'w') as f:
        f.write(api)

    with open('opal_proto.h', 'r') as f:
        proto = f.read()
    proto = re.sub(r'#include <linux/.*?>\n', '// removed\n', proto)
    with open('opal_proto.h', 'w') as f:
        f.write(proto)

if __name__ == "__main__":
    main()
