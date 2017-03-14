FROM node:6-alpine

ENV       NODE_ENV        production
RUN       mkdir /app
ADD       insert-indexes.js   /app/
ADD       package.json        /app/
ADD       npm-shrinkwrap.json /app/
WORKDIR   /app
RUN       npm install && npm cache clean

ENTRYPOINT ["node"]
