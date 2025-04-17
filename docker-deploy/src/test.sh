#!/bin/bash

# Start the proxy (assuming it's compiled as "./main")
./main 12345 &
PROXY_PID=$!

# Wait for proxy to start
sleep 2

echo "Testing GET request..."
curl -s -x http://localhost:12345 http://httpbin.org/forms/post > /dev/null

echo "Testing POST request..."
curl -s -x http://localhost:12345 -d "custname=Jerry&custtel=1234567890&custemail=123456789@qq.com&size=medium&topping=bacon&topping=cheese&delivery=&comments=" http://httpbin.org/post > /dev/null

echo "Testing CONNECT (HTTPS)..."
curl -s -x http://localhost:12345 https://www.facebook.com/ > /dev/null

echo "Testing chunked encoding..."
curl -s -x http://localhost:12345 http://www.httpwatch.com/httpgallery/chunked/chunkedimage.aspx > /dev/null

echo "Testing ETag validation (first request)..."
curl -s -x http://localhost:12345 http://run.mocky.io/v3/0f43637d-ca02-44c6-bfc9-ae3404c95c0d > /dev/null

echo "Testing ETag validation (second request - should hit cache)..."
curl -s -x http://localhost:12345 http://run.mocky.io/v3/0f43637d-ca02-44c6-bfc9-ae3404c95c0d > /dev/null

echo "Testing Last-Modified validation (first request)..."
curl -s -x http://localhost:12345 http://run.mocky.io/v3/0c2d20a9-0fb0-4a33-a10d-8181974a80d9 > /dev/null

echo "Testing Last-Modified validation (second request - should hit cache)..."
curl -s -x http://localhost:12345 http://run.mocky.io/v3/0c2d20a9-0fb0-4a33-a10d-8181974a80d9 > /dev/null

echo "Test for 502 Bad gateway..."
curl -s -x http://localhost:12345 http://run.mocky.io/v3/c2978b1a-63c4-4a2d-aec1-10b148ebe937 > /dev/null

echo "Test for 400 Bad Request..."
curl -s -x http://localhost:12345 http://run.mocky.io/v3/ec6c1606-5dc4-4461-a2cf-5fed1713222b > /dev/null

echo "Test for 429 Too Many Requests..."
curl -s -x http://localhost:12345 http://run.mocky.io/v3/4fb2ee14-334e-494c-8d7c-eb11f6f01ec2 > /dev/null

echo "Test for 204 No Content..."
curl -s -x http://localhost:12345 http://run.mocky.io/v3/18149ac2-b458-418a-af2e-efc2301071e9 > /dev/null

# Kill the proxy
kill $PROXY_PID

# Check the log file
echo "Proxy log file contents:"
cat var/log/erss/proxy.log