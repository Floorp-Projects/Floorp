def document_dimensions(session):
    return tuple(
        session.execute_script(
            """
        const {devicePixelRatio} = window;
        const width = document.documentElement.scrollWidth;
        const height = document.documentElement.scrollHeight;

        return [Math.floor(width * devicePixelRatio), Math.floor(height * devicePixelRatio)];
        """
        )
    )
