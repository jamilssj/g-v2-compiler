
funcao [
	LeVetor(vet[]:int):int
		[
			i:int;
		]
		{	
			i=0;
			enquanto (i<5) {
				escreva "Digite o valor do elemento";
				escreva i;
				escreva " do vetor" ;
				leia vet[i];
				i=i+1;
			}
		}
	
	somaVetor(v1[]:int, v2[]:int, v3[]:int):int
		[
			i: int;
		]
		{
			i=0;     
			enquanto(i<5) {
				v3[i]=v1[i]+v2[i];
				i=i+1;
			}
		}		

	imprimeVetor(v[]:int): int
		[
			i:int;
		]	
		{
			i=0;
			enquanto(i<5) {
				escreva "O valor do elemento ";
				escreva i; 
				escreva " do vetor:";
				escreva v[i];			
				novalinha;
				i=i+1;
			}
		}
]
principal
     [
		v1[5],v2[5],vsoma[5]: int;
	 ]
	 {
		escreva "Leitura do primeiro vetor";
		novalinha;
		LeVetor (v1);
		escreva "Leitura do segundo vetor";
		novalinha;
		LeVetor(v2);
		somaVetor(v1,v2,vsoma);
		escreva "O vetor soma:";
		novalinha;
		imprimeVetor(vsoma);
     }

