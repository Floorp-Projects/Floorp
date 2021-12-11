def api(root_url, service, version, path):
    """Generate URL for path in a Taskcluster service."""
    root_url = normalize_root_url(root_url)
    path = path.lstrip('/')
    return '{}/api/{}/{}/{}'.format(root_url, service, version, path)

def api_reference(root_url, service, version):
    """Generate URL for a Taskcluster api reference."""
    root_url = normalize_root_url(root_url)
    return '{}/references/{}/{}/api.json'.format(root_url, service, version)

def docs(root_url, path):
    """Generate URL for path in the Taskcluster docs."""
    root_url = normalize_root_url(root_url)
    path = path.lstrip('/')
    return '{}/docs/{}'.format(root_url, path)

def exchange_reference(root_url, service, version):
    """Generate URL for a Taskcluster exchange reference."""
    root_url = normalize_root_url(root_url)
    return '{}/references/{}/{}/exchanges.json'.format(root_url, service, version)

def schema(root_url, service, name):
    """Generate URL for a schema in a Taskcluster service."""
    root_url = normalize_root_url(root_url)
    name = name.lstrip('/')
    return '{}/schemas/{}/{}'.format(root_url, service, name)

def api_reference_schema(root_url, version):
    """Generate URL for the api reference schema."""
    return schema(root_url, 'common', 'api-reference-{}.json'.format(version))

def exchanges_reference_schema(root_url, version):
    """Generate URL for the exchanges reference schema."""
    return schema(root_url, 'common', 'exchanges-reference-{}.json'.format(version))

def api_manifest_schema(root_url, version):
    """Generate URL for the api manifest schema"""
    return schema(root_url, 'common', 'manifest-{}.json'.format(version))

def metadata_metaschema(root_url, version):
    """Generate URL for the metadata metaschema"""
    return schema(root_url, 'common', 'metadata-metaschema.json')

def ui(root_url, path):
    """Generate URL for a path in the Taskcluster ui."""
    root_url = normalize_root_url(root_url)
    path = path.lstrip('/')
    return '{}/{}'.format(root_url, path)

def api_manifest(root_url):
    """Returns a URL for the API manifest of a taskcluster deployment."""
    root_url = normalize_root_url(root_url)
    return '{}/references/manifest.json'.format(root_url)

def test_root_url():
    """Returns a standardized "testing" rootUrl that does not resolve but
    is easily recognizable in test failures."""
    return 'https://tc-tests.example.com'

def normalize_root_url(root_url):
    """Return the normal form of the given rootUrl"""
    return root_url.rstrip('/')
