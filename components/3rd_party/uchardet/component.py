
def add_file_downloads(confs : dict) -> list:
    '''
        @param confs kconfig vars, dict type
        @return list type, items is dict type
    '''
    version = f"{confs['CONFIG_UCHARDET_VERSION_MAJOR']}.{confs['CONFIG_UCHARDET_VERSION_MINOR']}.{confs['CONFIG_UCHARDET_VERSION_PATCH']}"
    url = f"https://github.com/BYVoid/uchardet/archive/refs/tags/v{version}.tar.gz"
    if version == "0.0.5":
        sha256sum = "7c5569c8ee1a129959347f5340655897e6a8f81ec3344de0012a243f868eabd1"
    else:
        raise Exception(f"version {version} not support")
    sites = ["https://github.com/BYVoid/uchardet"]
    filename = f"uchardet-{version}.tar.gz"
    path = f"uchardet_srcs"
    check_file = f'uchardet-{version}'
    rename = {}
    return [
        {
            'url': f'{url}',
            'urls': [],
            'sites': sites,
            'sha256sum': sha256sum,
            'filename': filename,
            'path': path,
            'check_files': [
                check_file
            ],
            'rename': rename
        }
    ]


