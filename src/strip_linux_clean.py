import re

def process():
    with open('opal.c', 'r') as f:
        content = f.read()

    # Insert headers
    header_block = """
#include "linux_xcompat.h"
#include "opal_api.h"
#include "opal.h"
#include "opal_proto.h"
"""
    content = header_block + content

    # Find the sed_ioctl function and just end the file there!
    # sed_ioctl is the last large chunk in the file and we don't need it.
    idx = content.find('int sed_ioctl(struct opal_dev *dev')
    if idx != -1:
        content = content[:idx]

    # Delete module_init / late_initcall
    content = re.sub(r'static int __init sed_opal_init.*', '', content, flags=re.DOTALL)
    
    # We still need to export the setup functions that user would call, drop 'static ' from them
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
        "free_opal_dev"
    ]
    for method in funcs_to_export:
        content = re.sub(r'static int ' + method + r'\(', r'int ' + method + '(', content)
        content = re.sub(r'static void ' + method + r'\(', r'void ' + method + '(', content)
        content = re.sub(r'static struct opal_dev \*' + method + r'\(', r'struct opal_dev *' + method + '(', content)

    # Some variables like sed_opal_keyring need to be commented out if they trigger errors, but our stub handles them
    with open('opal.c', 'w') as f:
        f.write(content)

    # Fix uapi_sed-opal.h -> opal_api.h
    with open('uapi_sed-opal.h', 'r') as f:
        api = f.read()
    api = re.sub(r'#include <linux/types.h>', '', api)
    api = re.sub(r'__u', 'uint', api)
    api = re.sub(r'__be', 'uint', api)
    api = re.sub(r'__le', 'uint', api)
    # Remove _IOR / _IOW ioctl definitions
    api = re.sub(r'#define IOC_OPAL_.*?_IOW?.*?$|#define IOC_OPAL_.*?_IOR?.*?$', '', api, flags=re.MULTILINE)
    with open('opal_api.h', 'w') as f:
        f.write(api)

    # Fix linux_sed-opal.h -> opal.h
    with open('linux_sed-opal.h', 'r') as f:
        lin = f.read()
    lin = re.sub(r'#include <linux/types.h>', '', lin)
    lin = re.sub(r'#include <linux/compiler_types.h>', '', lin)
    lin = re.sub(r'#include <uapi/linux/sed-opal.h>', '', lin)
    with open('opal.h', 'w') as f:
        f.write(lin)

process()
