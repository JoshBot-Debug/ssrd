# SSRD

A secure socket remote desktop. This project is not complete but is close enough

## How to build

Pull the project and build it. Done.
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

## How to use

In the build folder you will find
ssrd-server
ssrd-client

Run the server on your target machine
```bash
ssrd-server
```

Create a .ssrd folder in your user directory
Copy the public.pem key from the client into the .ssrd folder

From the client:
```bash
ssrd-client -h <ip-address>
```

## Generate the private & public openssl keys

Generate the private key, then extract the public key.

```bash
openssl genpkey -algorithm RSA -out private.pem
openssl rsa -in private.pem -pubout -out public.pem
```

##