
def add_file_downloads(confs : dict) -> list:
    '''
        @param confs kconfig vars, dict type
        @return list type, items is dict type
    '''
    version = f"{confs['CONFIG_LIBJUICE_VERSION_MAJOR']}.{confs['CONFIG_LIBJUICE_VERSION_MINOR']}.{confs['CONFIG_LIBJUICE_VERSION_PATCH']}"
    url = f"https://github.com/paullouisageneau/libjuice/archive/refs/tags/v{version}.tar.gz"
    if version == "1.7.0":
        sha256sum = "a510c7df90d82731d1d5e32e32205d3370ec2e62d6230ffe7b19b0f3c1acabf2"
    else:
        raise Exception(f"version {version} not support")
    sites = ["https://github.com/paullouisageneau/libjuice"]
    filename = f"libjuice-{version}.tar.gz"
    path = f"libjuice_srcs"
    check_file = f'libjuice-{version}'
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


