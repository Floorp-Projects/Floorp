import taskcluster


class VariantApk:
    def __init__(self, build_type, abi, engine, file_name):
        self.abi = abi
        self.taskcluster_path = 'public/build/{}/{}/target.apk'.format(abi, engine)
        self.absolute_path = '/opt/fenix/app/build/outputs/apk/{}/{}/{}'.format(engine, build_type, file_name)


class Variant:
    def __init__(self, name, build_type, apks):
        self.name = name
        self.build_type = build_type
        self._apks = apks

    def artifacts(self):
        return {
            apk.taskcluster_path: {
                'type': 'file',
                'path': apk.absolute_path,
                'expires': taskcluster.stringDate(taskcluster.fromNow('1 year')),
            } for apk in self._apks
        }

    def upstream_artifacts(self):
        return [apk.taskcluster_path for apk in self._apks]
