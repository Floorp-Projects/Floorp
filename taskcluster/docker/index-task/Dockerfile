FROM node:10-alpine

ENV       NODE_ENV        production
RUN       mkdir /app
ADD       insert-indexes.js   /app/
ADD       package.json        /app/
ADD       yarn.lock           /app/
WORKDIR   /app
RUN       yarn --frozen-lockfile && yarn cache clean

ENTRYPOINT ["node"]
