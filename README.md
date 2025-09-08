# SSRD

A secure socket remote desktop

## Generate the private & public openssl keys

Generate the private key, then extract the public key.

```bash
openssl genpkey -algorithm RSA -out private.pem
openssl rsa -in private.pem -pubout -out public.pem
```