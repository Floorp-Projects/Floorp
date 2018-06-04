from hglib.util import b

changeset = b('{rev}\\0{node}\\0{tags}\\0{branch}\\0{author}'
              '\\0{desc}\\0{date}\\0')
