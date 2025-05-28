id=$(docker create witch)
docker cp $id:/opt/witch/install .
docker rm -v $id

rm -rf witch
mv install witch
rm -rf witch/.git
zip -r witch witch
